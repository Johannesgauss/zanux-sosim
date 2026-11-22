#include "sys/exec.h"
#include "util/alias.h"
#include "util/memory.h"
#include "fs/fat32.h"
#include "mem/kmalloc.h"
#include "sched/proc.h"
#include "sched/proc_util.h"
#include "drivers/gfx/graphical_limine_util.h"
#include "drivers/kb/kb.h"
#include "drivers/port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char s_exec_args[256] = {0};

static bool to_fat83(const char *in, char out[FAT32_FILENAME_SIZE])
{
	memset(out, ' ', FAT32_FILENAME_SIZE);
	size_t i = 0;
	while (*in && *in != '.' && i < 8) {
		char c = *in++;
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		out[i++] = c;
	}
	if (i == 0)
		return false;
	if (*in == '.') {
		in++;
		/* Map 4-char ".json"/".JSON" cleanly to FAT32 8.3 "JSN". */
		if ((in[0] == 'j' || in[0] == 'J') &&
		    (in[1] == 's' || in[1] == 'S') &&
		    (in[2] == 'o' || in[2] == 'O') &&
		    (in[3] == 'n' || in[3] == 'N') &&
		    in[4] == '\0') {
			out[8] = 'J';
			out[9] = 'S';
			out[10] = 'N';
			return true;
		}
		size_t j = 0;
		while (*in && j < 3) {
			char c = *in++;
			if (c >= 'a' && c <= 'z')
				c = (char)(c - 32);
			out[8 + j++] = c;
		}
	}
	return *in == '\0';
}

int64_t sys_exec(const char *name)
{
	if (!name || !name[0])
		return -1;

	/* Split "<BINARY> [args...]" so Ring 3 apps can read CLI flags via sys_get_args(). */
	char bin[32];
	size_t blen = 0;
	while (name[blen] && name[blen] != ' ' && name[blen] != '\t' && blen + 1 < sizeof(bin)) {
		bin[blen] = name[blen];
		blen++;
	}
	bin[blen] = '\0';

	const char *args = name + blen;
	while (*args == ' ' || *args == '\t')
		args++;

	char name83[FAT32_FILENAME_SIZE];
	if (!to_fat83(bin, name83))
		return -1;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	struct file f = fat32_read_file(root, name83);
	kfree(root);
	if (!f.mptr || f.size == 0)
		return -1;

	size_t alen = 0;
	while (args[alen] && alen + 1 < sizeof(s_exec_args)) {
		s_exec_args[alen] = args[alen];
		alen++;
	}
	s_exec_args[alen] = '\0';

	struct proc *p = proc_create(f.mptr, f.size, 65536);
	fat32_close_file(f);
	if (p && current_proc)
		current_proc->state = PROC_BLOCK;
	return p ? (int64_t)p->id : -1;
}

int64_t sys_alloc(size_t size)
{
	return (int64_t)(uintptr_t)usr_kmalloc(size);
}

int64_t sys_read_file(const char *name, void *dst, size_t max_len)
{
	if (!name || !name[0])
		return -1;

	char name83[FAT32_FILENAME_SIZE];
	if (!to_fat83(name, name83))
		return -1;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	if (!dst || max_len == 0) {
		int64_t sz = fat32_file_size(root, name83);
		kfree(root);
		return sz;
	}

	struct file f = fat32_read_file(root, name83);
	kfree(root);
	if (!f.mptr || f.size == 0)
		return -1;

	size_t to_copy = (f.size < max_len) ? f.size : max_len;
	memcpy(dst, f.mptr, to_copy);
	fat32_close_file(f);
	return (int64_t)to_copy;
}

int64_t sys_fb_blit(const uint32_t *buf, uint32_t w, uint32_t h)
{
	if (!buf || w == 0 || h == 0 || !framebuffer_request.response || framebuffer_request.response->framebuffer_count < 1)
		return -1;

	struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
	uint32_t fb_w = (uint32_t)fb->width;
	uint32_t fb_h = (uint32_t)fb->height;
	uint32_t pitch_px = (uint32_t)(fb->pitch / sizeof(uint32_t));
	uint32_t *dst = (uint32_t *)fb->address;

	static uint32_t s_last_w = 0;
	static uint32_t s_last_h = 0;
	if (w != s_last_w || h != s_last_h) {
		memset(dst, 0, fb->pitch * fb->height);
		s_last_w = w;
		s_last_h = h;
	}

	static uint32_t s_row_buf[4096];
	uint32_t sx = fb_w / w;
	uint32_t sy = fb_h / h;
	uint32_t scale = (sx < sy) ? sx : sy;
	if (scale < 1)
		scale = 1;
	while (scale > 1 && w * scale > 4096)
		scale--;

	uint32_t out_w = w * scale;
	uint32_t out_h = h * scale;
	uint32_t ox = (out_w < fb_w) ? ((fb_w - out_w) / 2) : 0;
	uint32_t oy = (out_h < fb_h) ? ((fb_h - out_h) / 2) : 0;

	if (scale == 1) {
		uint32_t copy_w = (w < fb_w) ? w : fb_w;
		uint32_t copy_h = (h < fb_h) ? h : fb_h;
		for (uint32_t y = 0; y < copy_h; y++)
			memcpy(dst + (oy + y) * pitch_px + ox, buf + y * w, copy_w * sizeof(uint32_t));
		return 0;
	}

	for (uint32_t y = 0; y < h; y++) {
		const uint32_t *src_row = buf + y * w;
		uint32_t *r = s_row_buf;
		for (uint32_t x = 0; x < w; x++) {
			uint32_t px = src_row[x];
			for (uint32_t rx = 0; rx < scale; rx++)
				*r++ = px;
		}
		uint32_t *dst_base = dst + (oy + y * scale) * pitch_px + ox;
		for (uint32_t ry = 0; ry < scale; ry++) {
			uint64_t *d64 = (uint64_t *)(dst_base + ry * pitch_px);
			const uint64_t *s64 = (const uint64_t *)s_row_buf;
			uint32_t qwords = out_w / 2;
			for (uint32_t q = 0; q < qwords; q++)
				d64[q] = s64[q];
			if (out_w & 1)
				(dst_base + ry * pitch_px)[out_w - 1] = s_row_buf[out_w - 1];
		}
	}
	return 0;
}

int64_t sys_poll_scancode(void)
{
	return (int64_t)kb_pop_scancode();
}

extern volatile uint64_t g_timer_ticks;

int64_t sys_ticks_ms(void)
{
	// PIT runs at 100 Hz (10 ms per IRQ0 tick).
	return (int64_t)(g_timer_ticks * 10ULL);
}

int64_t sys_pcspk(uint16_t divisor)
{
	if (divisor == 0) {
		uint8_t tmp = inb(0x61);
		if (tmp & 0x03)
			outb(0x61, (uint8_t)(tmp & 0xFC));
		return 0;
	}
	outb(0x43, 0xB6);
	outb(0x42, (uint8_t)(divisor & 0xFF));
	outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));
	uint8_t tmp = inb(0x61);
	if ((tmp & 0x03) != 0x03)
		outb(0x61, (uint8_t)(tmp | 0x03));
	return 0;
}

int64_t sys_opl_io(uint16_t port, uint8_t val, int is_write)
{
	if (port < 0x388 || port > 0x38B)
		return -1;
	if (is_write) {
		outb(port, val);
		return 0;
	}
	return (int64_t)inb(port);
}

static void format_fat83(const uint8_t raw[11], char out[16])
{
	size_t pos = 0;
	size_t base_end = 8;
	while (base_end > 0 && raw[base_end - 1] == ' ')
		base_end--;
	for (size_t i = 0; i < base_end; i++)
		out[pos++] = (char)raw[i];

	size_t ext_end = 3;
	while (ext_end > 0 && raw[8 + ext_end - 1] == ' ')
		ext_end--;
	if (ext_end > 0) {
		out[pos++] = '.';
		for (size_t i = 0; i < ext_end; i++)
			out[pos++] = (char)raw[8 + i];
	}
	out[pos] = '\0';
}

int64_t sys_readdir(size_t index, char name_out[16], uint32_t *size_out)
{
	if (!name_out)
		return -1;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	if (!root)
		return -1;

	size_t max_entries = fat32_dir_entries_count();
	size_t seen = 0;
	for (size_t i = 0; i < max_entries && root[i].filename[0] != 0x00; i++) {
		if (root[i].filename[0] == 0xE5)
			continue;
		if ((root[i].attr & 0x0F) == 0x0F)
			continue;
		if (root[i].attr & 0x08)
			continue;
		if (seen == index) {
			format_fat83(root[i].filename, name_out);
			if (size_out)
				*size_out = root[i].size;
			kfree(root);
			return 1;
		}
		seen++;
	}

	kfree(root);
	return 0;
}

#include "sched/sim/sim_engine.h"

int64_t sys_sim_start(const void *cfg)
{
	return sim_engine_start((const struct sim *)cfg, current_proc);
}

int64_t sys_sim_poll(void *out_trace)
{
	return sim_engine_poll((struct sim *)out_trace);
}

#include "drivers/audio/sb16.h"

int64_t sys_audio_write(const uint8_t *pcm, size_t len)
{
	if (!pcm) {
		sb16_silence();
		return 0;
	}
	return sb16_write_pcm(pcm, len);
}

int64_t sys_get_args(char *dst, size_t max_len)
{
	if (!dst || max_len == 0)
		return -1;
	size_t i = 0;
	while (s_exec_args[i] && i + 1 < max_len) {
		dst[i] = s_exec_args[i];
		i++;
	}
	dst[i] = '\0';
	return (int64_t)i;
}

int64_t sys_win_blit(const uint32_t *buf, uint32_t w, uint32_t h)
{
	return sim_engine_win_blit(current_proc, buf, w, h);
}

int64_t sys_win_read(uint32_t slot, uint32_t *dst, uint32_t max_pixels)
{
	return sim_engine_win_read(slot, dst, max_pixels);
}

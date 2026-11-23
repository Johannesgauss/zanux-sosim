#include "syscall.h"
#include <zgfx.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define FB_W 640U
#define FB_H 400U
#define CELL_W 8U
#define CELL_H 8U

#define SIDEBAR_COLS 16U
#define SIDEBAR_PX   (SIDEBAR_COLS * CELL_W) /* 128px */
#define GUTTER_COLS  4U
#define ED_X         (SIDEBAR_PX + GUTTER_COLS * CELL_W) /* 160px */
#define ED_COLS      ((FB_W - ED_X) / CELL_W) /* 60 cols */
#define ED_ROWS      46U /* y = 16..383 (row 0..1 header, row 48 status, row 49 cmd) */

#define MAX_LINES    256U
#define MAX_LINE_LEN 160U
#define MAX_FILES    64U
#define IO_BUF_CAP   (MAX_LINES * MAX_LINE_LEN)

/* Palette (dark Neovim-inspired theme) */
#define C_BG        0x0012161CU
#define C_SIDEBAR   0x000E1117U
#define C_BORDER    0x002B3544U
#define C_CUR_LINE  0x001B222CU
#define C_SEL_BG    0x0023364AU
#define C_TEXT      0x00E6EDF3U
#define C_DIM       0x006E7681U
#define C_GUTTER    0x00525C68U
#define C_GREEN     0x002ECC71U
#define C_BLUE      0x003498DBU
#define C_YELLOW    0x00F1C40FU
#define C_CYAN      0x0000BCD4U
#define C_PURPLE    0x00B072D1U
#define C_RED       0x00E74C3CU
#define C_STATUS_BG 0x001F2937U

enum vim_mode {
	MODE_NORMAL  = 0,
	MODE_INSERT  = 1,
	MODE_COMMAND = 2,
	MODE_FILES   = 3
};

static uint32_t s_fb[FB_W * FB_H];
static char s_lines[MAX_LINES][MAX_LINE_LEN];
static uint32_t s_nlines = 1;
static uint32_t s_cx = 0, s_cy = 0;
static uint32_t s_scroll_y = 0, s_scroll_x = 0;

static char s_filename[32] = "CASO1.JSN";
static bool s_dirty = false;
static enum vim_mode s_mode = MODE_NORMAL;
static char s_cmd[64];
static uint32_t s_cmd_len = 0;
static char s_msg[80];
static uint32_t s_msg_col = C_TEXT;

static char s_files[MAX_FILES][16];
static uint32_t s_file_sizes[MAX_FILES];
static uint32_t s_nfiles = 0;
static uint32_t s_file_sel = 0;
static uint32_t s_file_scroll = 0;

static char s_io_buf[IO_BUF_CAP];
static bool s_shift = false;
static bool s_ctrl = false;
static bool s_pending_d = false;

static const char s_json_template[] =
	"{\n"
	"  \"quantum\": 2,\n"
	"  \"sobrecarga_contexto\": 1,\n"
	"  \"custo_disco\": 3,\n"
	"  \"seed\": 42,\n"
	"  \"processos\": [\n"
	"    {\"id\": \"P1\", \"bin_name\": \"SORT.BIN\",   \"chegada\": 0, \"execucao\": 5, \"deadline\": 8,  \"prioridade\": 2, \"num_paginas\": 4},\n"
	"    {\"id\": \"P2\", \"bin_name\": \"LIFE.BIN\",   \"chegada\": 1, \"execucao\": 4, \"deadline\": 12, \"prioridade\": 1, \"num_paginas\": 4},\n"
	"    {\"id\": \"P3\", \"bin_name\": \"PLASMA.BIN\", \"chegada\": 3, \"execucao\": 3, \"deadline\": 18, \"prioridade\": 3, \"num_paginas\": 3}\n"
	"  ]\n"
	"}\n";

/* US QWERTY PS/2 Set-1 scancode tables */
static const char s_sc_normal[128] = {
	0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', 0,  0,
	'q','w','e','r','t','y','u','i','o','p','[',']', 0,  0, 'a','s',
	'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
	'b','n','m',',','.','/', 0, '*', 0, ' ', 0,  0,  0,  0,  0,  0
};

static const char s_sc_shift[128] = {
	0,  0, '!','@','#','$','%','^','&','*','(',')','_','+', 0,  0,
	'Q','W','E','R','T','Y','U','I','O','P','{','}', 0,  0, 'A','S',
	'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
	'B','N','M','<','>','?', 0, '*', 0, ' ', 0,  0,  0,  0,  0,  0
};

static void set_msg(const char *m, uint32_t col)
{
	size_t i = 0;
	while (m[i] && i + 1 < sizeof(s_msg)) {
		s_msg[i] = m[i];
		i++;
	}
	s_msg[i] = '\0';
	s_msg_col = col;
}

static void refresh_files(void)
{
	s_nfiles = 0;
	for (size_t idx = 0; s_nfiles < MAX_FILES; idx++) {
		char name[16];
		uint32_t sz = 0;
		int64_t r = sys_readdir(idx, name, &sz);
		if (r <= 0)
			break;
		memcpy(s_files[s_nfiles], name, 16);
		s_file_sizes[s_nfiles] = sz;
		s_nfiles++;
	}
	if (s_file_sel >= s_nfiles && s_nfiles > 0)
		s_file_sel = s_nfiles - 1U;
}

static bool is_text_ext(const char *name)
{
	size_t n = strlen(name);
	if (n >= 4) {
		const char *ext = name + n - 4;
		if (strcmp(ext, ".JSN") == 0 || strcmp(ext, ".TXT") == 0 || strcmp(ext, ".LOG") == 0)
			return true;
	}
	return false;
}

static void load_text_into_buffer(const char *text, size_t len)
{
	s_nlines = 0;
	uint32_t col = 0;
	s_lines[0][0] = '\0';

	for (size_t i = 0; i < len && s_nlines < MAX_LINES; i++) {
		char ch = text[i];
		if (ch == '\r')
			continue;
		if (ch == '\n') {
			s_lines[s_nlines][col] = '\0';
			s_nlines++;
			col = 0;
			if (s_nlines < MAX_LINES)
				s_lines[s_nlines][0] = '\0';
		} else if (ch == '\t') {
			for (int sp = 0; sp < 2 && col + 1 < MAX_LINE_LEN; sp++)
				s_lines[s_nlines][col++] = ' ';
		} else if ((unsigned char)ch >= 0x20 && (unsigned char)ch < 0x7F) {
			if (col + 1 < MAX_LINE_LEN)
				s_lines[s_nlines][col++] = ch;
		}
	}
	if (s_nlines == 0) {
		s_lines[0][col] = '\0';
		s_nlines = 1;
	} else if (col > 0 && s_nlines < MAX_LINES) {
		s_lines[s_nlines][col] = '\0';
		s_nlines++;
	}

	s_cx = 0;
	s_cy = 0;
	s_scroll_x = 0;
	s_scroll_y = 0;
	s_dirty = false;
}

static void open_file(const char *name)
{
	size_t i = 0;
	while (name[i] && i + 1 < sizeof(s_filename)) {
		s_filename[i] = name[i];
		i++;
	}
	s_filename[i] = '\0';

	int64_t n = sys_read_file(s_filename, s_io_buf, sizeof(s_io_buf) - 1);
	if (n < 0) {
		load_text_into_buffer("", 0);
		set_msg("Novo arquivo (use :tpl para modelo JSON, :w para salvar)", C_YELLOW);
		return;
	}
	load_text_into_buffer(s_io_buf, (size_t)n);
	snprintf(s_msg, sizeof(s_msg), "Aberto \"%s\" (%u linhas)", s_filename, s_nlines);
	s_msg_col = C_GREEN;
}

static bool save_file(const char *override_name)
{
	if (override_name && override_name[0]) {
		size_t i = 0;
		while (override_name[i] && i + 1 < sizeof(s_filename)) {
			s_filename[i] = override_name[i];
			i++;
		}
		s_filename[i] = '\0';
	}

	size_t pos = 0;
	for (uint32_t r = 0; r < s_nlines; r++) {
		size_t ll = strlen(s_lines[r]);
		if (pos + ll + 2 >= sizeof(s_io_buf))
			break;
		memcpy(s_io_buf + pos, s_lines[r], ll);
		pos += ll;
		s_io_buf[pos++] = '\n';
	}

	int64_t written = sys_write_file(s_filename, s_io_buf, pos);
	if (written < 0) {
		snprintf(s_msg, sizeof(s_msg), "ERRO ao salvar \"%s\" (use nome 8.3 ou .JSON)", s_filename);
		s_msg_col = C_RED;
		return false;
	}
	s_dirty = false;
	refresh_files();
	snprintf(s_msg, sizeof(s_msg), "\"%s\" salvo (%u bytes)!", s_filename, (uint32_t)written);
	s_msg_col = C_GREEN;
	return true;
}

static void clamp_cursor(void)
{
	if (s_nlines == 0) {
		s_nlines = 1;
		s_lines[0][0] = '\0';
	}
	if (s_cy >= s_nlines)
		s_cy = s_nlines - 1U;
	uint32_t ll = (uint32_t)strlen(s_lines[s_cy]);
	uint32_t max_x = (s_mode == MODE_INSERT) ? ll : (ll > 0 ? ll - 1U : 0U);
	if (s_cx > max_x)
		s_cx = max_x;

	if (s_cy < s_scroll_y)
		s_scroll_y = s_cy;
	else if (s_cy >= s_scroll_y + ED_ROWS)
		s_scroll_y = s_cy - ED_ROWS + 1U;

	if (s_cx < s_scroll_x)
		s_scroll_x = s_cx;
	else if (s_cx >= s_scroll_x + ED_COLS)
		s_scroll_x = s_cx - ED_COLS + 1U;
}

static void insert_char(char ch)
{
	char *line = s_lines[s_cy];
	uint32_t ll = (uint32_t)strlen(line);
	if (ll + 1 >= MAX_LINE_LEN)
		return;
	for (uint32_t i = ll + 1; i > s_cx; i--)
		line[i] = line[i - 1U];
	line[s_cx++] = ch;
	s_dirty = true;
}

static void insert_newline(void)
{
	if (s_nlines >= MAX_LINES)
		return;

	/* Measure leading spaces for auto-indent. */
	uint32_t indent = 0;
	while (s_lines[s_cy][indent] == ' ' && indent < s_cx && indent < 16U)
		indent++;

	for (uint32_t r = s_nlines; r > s_cy + 1U; r--)
		memcpy(s_lines[r], s_lines[r - 1U], MAX_LINE_LEN);
	s_nlines++;

	char *cur = s_lines[s_cy];
	char *nxt = s_lines[s_cy + 1U];
	for (uint32_t i = 0; i < indent; i++)
		nxt[i] = ' ';
	size_t tail = strlen(cur + s_cx);
	if (indent + tail >= MAX_LINE_LEN)
		tail = MAX_LINE_LEN - indent - 1U;
	memcpy(nxt + indent, cur + s_cx, tail);
	nxt[indent + tail] = '\0';
	cur[s_cx] = '\0';

	s_cy++;
	s_cx = indent;
	s_dirty = true;
}

static void delete_line(uint32_t row)
{
	if (s_nlines <= 1U) {
		s_lines[0][0] = '\0';
		s_cx = 0;
		s_dirty = true;
		return;
	}
	for (uint32_t r = row; r + 1U < s_nlines; r++)
		memcpy(s_lines[r], s_lines[r + 1U], MAX_LINE_LEN);
	s_nlines--;
	s_dirty = true;
}

static void backspace_char(void)
{
	if (s_cx > 0) {
		char *line = s_lines[s_cy];
		uint32_t ll = (uint32_t)strlen(line);
		for (uint32_t i = s_cx - 1U; i < ll; i++)
			line[i] = line[i + 1U];
		s_cx--;
		s_dirty = true;
	} else if (s_cy > 0) {
		char *prev = s_lines[s_cy - 1U];
		char *cur = s_lines[s_cy];
		uint32_t pl = (uint32_t)strlen(prev);
		uint32_t cl = (uint32_t)strlen(cur);
		if (pl + cl < MAX_LINE_LEN) {
			memcpy(prev + pl, cur, cl + 1U);
			delete_line(s_cy);
			s_cy--;
			s_cx = pl;
		}
	}
}

static bool exec_command(void)
{
	char *p = s_cmd;
	while (*p == ' ') p++;

	if (strcmp(p, "q") == 0) {
		if (s_dirty) {
			set_msg("Alteracoes nao salvas! Use :q! para sair ou :wq para salvar.", C_RED);
			return false;
		}
		return true;
	}
	if (strcmp(p, "q!") == 0)
		return true;
	if (strcmp(p, "wq") == 0 || strcmp(p, "x") == 0)
		return save_file(NULL);
	if (strcmp(p, "w") == 0) {
		save_file(NULL);
		return false;
	}
	if (strncmp(p, "w ", 2) == 0) {
		char *arg = p + 2;
		while (*arg == ' ') arg++;
		if (*arg)
			save_file(arg);
		else
			save_file(NULL);
		return false;
	}
	if (strncmp(p, "e ", 2) == 0) {
		char *arg = p + 2;
		while (*arg == ' ') arg++;
		if (*arg)
			open_file(arg);
		return false;
	}
	if (strcmp(p, "tpl") == 0 || strcmp(p, "new") == 0) {
		load_text_into_buffer(s_json_template, sizeof(s_json_template) - 1U);
		s_dirty = true;
		set_msg("Modelo JSON inserido! Edite e salve com :w CASO3.JSON", C_CYAN);
		return false;
	}
	if (strncmp(p, "new ", 4) == 0) {
		char *arg = p + 4;
		while (*arg == ' ') arg++;
		if (*arg) {
			size_t i = 0;
			while (arg[i] && i + 1 < sizeof(s_filename)) {
				s_filename[i] = arg[i];
				i++;
			}
			s_filename[i] = '\0';
		}
		load_text_into_buffer(s_json_template, sizeof(s_json_template) - 1U);
		s_dirty = true;
		snprintf(s_msg, sizeof(s_msg), "Modelo JSON pronto para \"%s\" (:w para salvar)", s_filename);
		s_msg_col = C_CYAN;
		return false;
	}

	snprintf(s_msg, sizeof(s_msg), "Comando desconhecido: :%s", p);
	s_msg_col = C_RED;
	return false;
}

static void render_ui(void)
{
	struct zcanvas cv = { s_fb, FB_W, FB_H };
	zgfx_clear(&cv, C_BG);

	/* Left file sidebar background & divider */
	zgfx_rect(&cv, 0, 0, SIDEBAR_PX, FB_H - 16U, C_SIDEBAR);
	zgfx_vline(&cv, SIDEBAR_PX - 1U, 0, FB_H - 16U, (s_mode == MODE_FILES) ? C_YELLOW : C_BORDER);

	/* Top header bars */
	zgfx_rect(&cv, 0, 0, SIDEBAR_PX - 1U, 14U, (s_mode == MODE_FILES) ? 0x003D3212U : 0x001B222CU);
	zgfx_text(&cv, 6, 3, "FILES [Tab]", (s_mode == MODE_FILES) ? C_YELLOW : C_CYAN, 1);

	zgfx_rect(&cv, SIDEBAR_PX, 0, FB_W - SIDEBAR_PX, 14U, 0x001B222CU);
	zgfx_printf(&cv, SIDEBAR_PX + 8U, 3, C_TEXT, 1, "Zanux Vim  |  %s%s",
	            s_filename, s_dirty ? " [+]" : "");
	zgfx_text(&cv, FB_W - 148U, 3, ":tpl :w :wq :q!", C_DIM, 1);

	/* Sidebar file list */
	if (s_file_sel < s_file_scroll)
		s_file_scroll = s_file_sel;
	else if (s_file_sel >= s_file_scroll + ED_ROWS)
		s_file_scroll = s_file_sel - ED_ROWS + 1U;

	for (uint32_t row = 0; row < ED_ROWS && (s_file_scroll + row) < s_nfiles; row++) {
		uint32_t idx = s_file_scroll + row;
		uint32_t y = 16U + row * CELL_H;
		bool is_sel = (idx == s_file_sel);
		bool is_open = (strcmp(s_files[idx], s_filename) == 0);

		if (is_sel && s_mode == MODE_FILES)
			zgfx_rect(&cv, 0, y, SIDEBAR_PX - 1U, CELL_H, C_SEL_BG);
		else if (is_open)
			zgfx_rect(&cv, 0, y, SIDEBAR_PX - 1U, CELL_H, 0x00192533U);

		uint32_t col = is_text_ext(s_files[idx]) ? C_GREEN : C_DIM;
		if (is_sel && s_mode == MODE_FILES) col = C_YELLOW;
		else if (is_open) col = C_TEXT;

		zgfx_char(&cv, 2, y, is_open ? '>' : ' ', C_CYAN, 1);
		char short_name[15];
		size_t k = 0;
		while (s_files[idx][k] && k < 14) {
			short_name[k] = s_files[idx][k];
			k++;
		}
		short_name[k] = '\0';
		zgfx_text(&cv, 12, y, short_name, col, 1);
	}

	/* Editor rows */
	for (uint32_t row = 0; row < ED_ROWS; row++) {
		uint32_t ln = s_scroll_y + row;
		uint32_t y = 16U + row * CELL_H;
		if (ln >= s_nlines) {
			zgfx_char(&cv, SIDEBAR_PX + 8U, y, '~', C_GUTTER, 1);
			continue;
		}
		if (ln == s_cy && s_mode != MODE_FILES)
			zgfx_rect(&cv, SIDEBAR_PX, y, FB_W - SIDEBAR_PX, CELL_H, C_CUR_LINE);

		/* Line number */
		zgfx_printf(&cv, SIDEBAR_PX + 2U, y, (ln == s_cy) ? C_YELLOW : C_GUTTER, 1, "%3u", ln + 1U);

		/* Line text with lightweight JSON syntax coloring */
		const char *s = s_lines[ln];
		uint32_t ll = (uint32_t)strlen(s);
		bool in_str = false;
		for (uint32_t c = 0; c < s_scroll_x && c < ll; c++) {
			if (s[c] == '"' && (c == 0 || s[c - 1U] != '\\'))
				in_str = !in_str;
		}
		for (uint32_t col = 0; col < ED_COLS; col++) {
			uint32_t ci = s_scroll_x + col;
			if (ci >= ll)
				break;
			char ch = s[ci];
			uint32_t col_px = C_TEXT;
			if (ch == '"' && (ci == 0 || s[ci - 1U] != '\\')) {
				col_px = C_GREEN;
				in_str = !in_str;
			} else if (in_str) {
				col_px = C_GREEN;
			} else if (ch == '{' || ch == '}' || ch == '[' || ch == ']') {
				col_px = C_CYAN;
			} else if (ch >= '0' && ch <= '9') {
				col_px = C_YELLOW;
			} else if (ch == ':' || ch == ',') {
				col_px = C_PURPLE;
			}
			zgfx_char(&cv, ED_X + col * CELL_W, y, ch, col_px, 1);
		}

		/* Cursor */
		if (ln == s_cy && s_mode != MODE_FILES && s_mode != MODE_COMMAND) {
			uint32_t cur_col = (s_cx >= s_scroll_x) ? (s_cx - s_scroll_x) : 0U;
			if (cur_col < ED_COLS) {
				uint32_t px = ED_X + cur_col * CELL_W;
				if (s_mode == MODE_INSERT) {
					zgfx_rect(&cv, px, y, 2U, CELL_H, C_GREEN);
				} else {
					zgfx_rect(&cv, px, y, CELL_W, CELL_H, C_BLUE);
					char under = (s_cx < ll) ? s[s_cx] : ' ';
					zgfx_char(&cv, px, y, under, C_BG, 1);
				}
			}
		}
	}

	/* Status bar (y = 384..391) */
	uint32_t sy = FB_H - 16U;
	zgfx_rect(&cv, 0, sy, FB_W, 8U, C_STATUS_BG);
	const char *mname = "NORMAL";
	uint32_t mcol = C_BLUE;
	if (s_mode == MODE_INSERT)       { mname = "INSERT";  mcol = C_GREEN;  }
	else if (s_mode == MODE_COMMAND) { mname = "COMMAND"; mcol = C_PURPLE; }
	else if (s_mode == MODE_FILES)   { mname = "FILES";   mcol = C_YELLOW; }

	zgfx_rect(&cv, 0, sy, 68U, 8U, mcol);
	zgfx_text(&cv, 6, sy, mname, C_BG, 1);
	zgfx_printf(&cv, 76U, sy, C_TEXT, 1, "%s%s", s_filename, s_dirty ? " [+]" : "");
	zgfx_printf(&cv, FB_W - 140U, sy, C_CYAN, 1, "Ln %u/%u Col %u",
	            s_cy + 1U, s_nlines, s_cx + 1U);

	/* Bottom command / message line (y = 392..399) */
	uint32_t cy = FB_H - 8U;
	zgfx_rect(&cv, 0, cy, FB_W, 8U, C_BG);
	if (s_mode == MODE_COMMAND) {
		zgfx_printf(&cv, 4, cy, C_TEXT, 1, ":%s_", s_cmd);
	} else {
		zgfx_text(&cv, 4, cy, s_msg, s_msg_col, 1);
	}

	sys_fb_blit(s_fb, FB_W, FB_H);
}

int vim_main(void)
{
	refresh_files();

	char arg[64];
	int64_t alen = sys_get_args(arg, sizeof(arg));
	if (alen > 0) {
		char *p = arg;
		while (*p == ' ' || *p == '\t') p++;
		char *e = p;
		while (*e && *e != ' ' && *e != '\t') e++;
		*e = '\0';
		if (*p)
			open_file(p);
		else
			open_file("CASO1.JSN");
	} else {
		open_file("CASO1.JSN");
	}

	clamp_cursor();
	render_ui();

	for (;;) {
		int32_t sc = sys_poll_scancode();
		if (sc <= 0) {
			asm volatile("pause");
			continue;
		}

		/* Track Shift & Ctrl modifiers */
		if (sc == 0x2A || sc == 0x36) { s_shift = true; continue; }
		if (sc == 0xAA || sc == 0xB6) { s_shift = false; continue; }
		if (sc == 0x1D) { s_ctrl = true; continue; }
		if (sc == 0x9D) { s_ctrl = false; continue; }
		if (sc & 0x80) continue; /* Ignore key release */

		char ch = (sc < 128) ? (s_shift ? s_sc_shift[sc] : s_sc_normal[sc]) : 0;

		/* Global shortcuts: Ctrl+S (save), Ctrl+E (toggle file sidebar) */
		if (s_ctrl && (ch == 's' || ch == 'S')) {
			save_file(NULL);
			render_ui();
			continue;
		}
		if (s_ctrl && (ch == 'e' || ch == 'E')) {
			s_mode = (s_mode == MODE_FILES) ? MODE_NORMAL : MODE_FILES;
			if (s_mode == MODE_FILES) refresh_files();
			render_ui();
			continue;
		}

		if (s_mode == MODE_FILES) {
			if (sc == 0x01 || sc == 0x0F) { /* ESC or Tab */
				s_mode = MODE_NORMAL;
			} else if (ch == 'j' || sc == 0x50) { /* j / Down */
				if (s_file_sel + 1U < s_nfiles) s_file_sel++;
			} else if (ch == 'k' || sc == 0x48) { /* k / Up */
				if (s_file_sel > 0U) s_file_sel--;
			} else if (sc == 0x1C) { /* Enter: open selected file */
				if (s_file_sel < s_nfiles) {
					open_file(s_files[s_file_sel]);
					s_mode = MODE_NORMAL;
				}
			} else if (ch == 'n') { /* New JSON case */
				open_file("CASO3.JSN");
				load_text_into_buffer(s_json_template, sizeof(s_json_template) - 1U);
				s_dirty = true;
				set_msg("Novo CASO3.JSN com modelo JSON (:w para salvar)", C_CYAN);
				s_mode = MODE_NORMAL;
			} else if (ch == 'r') {
				refresh_files();
				set_msg("Lista de arquivos atualizada.", C_CYAN);
			} else if (ch == ':') {
				s_mode = MODE_COMMAND;
				s_cmd_len = 0;
				s_cmd[0] = '\0';
			}
		} else if (s_mode == MODE_NORMAL) {
			if (s_pending_d && ch != 'd')
				s_pending_d = false;

			if (sc == 0x0F) { /* Tab -> toggle sidebar */
				refresh_files();
				s_mode = MODE_FILES;
			} else if (ch == 'h' || sc == 0x4B) {
				if (s_cx > 0) s_cx--;
			} else if (ch == 'l' || sc == 0x4D) {
				s_cx++;
			} else if (ch == 'j' || sc == 0x50) {
				if (s_cy + 1U < s_nlines) s_cy++;
			} else if (ch == 'k' || sc == 0x48) {
				if (s_cy > 0) s_cy--;
			} else if (ch == '0') {
				s_cx = 0;
			} else if (ch == '$') {
				s_cx = (uint32_t)strlen(s_lines[s_cy]);
			} else if (ch == 'g') {
				s_cy = 0; s_cx = 0;
			} else if (ch == 'G') {
				s_cy = s_nlines - 1U; s_cx = 0;
			} else if (ch == 'i') {
				s_mode = MODE_INSERT;
				set_msg("-- INSERT -- (ESC para voltar ao NORMAL)", C_GREEN);
			} else if (ch == 'a') {
				if (s_cx < (uint32_t)strlen(s_lines[s_cy])) s_cx++;
				s_mode = MODE_INSERT;
				set_msg("-- INSERT --", C_GREEN);
			} else if (ch == 'A') {
				s_cx = (uint32_t)strlen(s_lines[s_cy]);
				s_mode = MODE_INSERT;
				set_msg("-- INSERT --", C_GREEN);
			} else if (ch == 'o') {
				s_cx = (uint32_t)strlen(s_lines[s_cy]);
				insert_newline();
				s_mode = MODE_INSERT;
				set_msg("-- INSERT --", C_GREEN);
			} else if (ch == 'O') {
				s_cx = 0;
				insert_newline();
				if (s_cy > 0) s_cy--;
				s_mode = MODE_INSERT;
				set_msg("-- INSERT --", C_GREEN);
			} else if (ch == 'x') {
				char *line = s_lines[s_cy];
				uint32_t ll = (uint32_t)strlen(line);
				if (ll > 0 && s_cx < ll) {
					for (uint32_t i = s_cx; i < ll; i++)
						line[i] = line[i + 1U];
					s_dirty = true;
				}
			} else if (ch == 'd') {
				if (s_pending_d) {
					delete_line(s_cy);
					s_pending_d = false;
				} else {
					s_pending_d = true;
				}
			} else if (ch == ':') {
				s_mode = MODE_COMMAND;
				s_cmd_len = 0;
				s_cmd[0] = '\0';
			}
		} else if (s_mode == MODE_INSERT) {
			if (sc == 0x01) { /* ESC */
				s_mode = MODE_NORMAL;
				if (s_cx > 0) s_cx--;
				set_msg("", C_TEXT);
			} else if (sc == 0x1C) { /* Enter */
				insert_newline();
			} else if (sc == 0x0E) { /* Backspace */
				backspace_char();
			} else if (sc == 0x0F) { /* Tab -> 2 spaces */
				insert_char(' ');
				insert_char(' ');
			} else if (sc == 0x4B) {
				if (s_cx > 0) s_cx--;
			} else if (sc == 0x4D) {
				if (s_cx < (uint32_t)strlen(s_lines[s_cy])) s_cx++;
			} else if (sc == 0x48) {
				if (s_cy > 0) s_cy--;
			} else if (sc == 0x50) {
				if (s_cy + 1U < s_nlines) s_cy++;
			} else if (ch >= 0x20 && ch < 0x7F) {
				insert_char(ch);
			}
		} else if (s_mode == MODE_COMMAND) {
			if (sc == 0x01) { /* ESC */
				s_mode = MODE_NORMAL;
			} else if (sc == 0x1C) { /* Enter */
				s_mode = MODE_NORMAL;
				if (exec_command())
					return 0;
			} else if (sc == 0x0E) { /* Backspace */
				if (s_cmd_len > 0)
					s_cmd[--s_cmd_len] = '\0';
				else
					s_mode = MODE_NORMAL;
			} else if (ch >= 0x20 && ch < 0x7F && s_cmd_len + 1U < sizeof(s_cmd)) {
				s_cmd[s_cmd_len++] = ch;
				s_cmd[s_cmd_len] = '\0';
			}
		}

		clamp_cursor();
		render_ui();
	}
}

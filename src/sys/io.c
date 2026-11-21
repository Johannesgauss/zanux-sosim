#include "sys/io.h"
#include "util/alias.h"
#include "drivers/gfx/console.h"
#include "drivers/kb/kb.h"
#include "drivers/port.h"

int64_t sys_write(int fd, const char *buf, size_t count)
{
	if (!buf)
		return -1;

	if (fd == FD_STDOUT) {
		for (size_t i = 0; i < count; i++) {
			if (buf[i] == '\x0C')
				gfx_clear_screen();
			else
				gfx_putc_color((uint8_t)buf[i], 51, 255, 51);
		}
		return (int64_t)count;
	}

	if (fd == FD_STDERR) {
		for (size_t i = 0; i < count; i++)
			gfx_putc_color((uint8_t)buf[i], 255, 80, 80);
		return (int64_t)count;
	}

	return -1;
}

int64_t sys_read(int fd, char *buf, size_t count)
{
	if (fd != FD_STDIN || !buf)
		return -1;
	if (count == 0)
		return 0;

	size_t n = 0;
	while (n < count) {
		int32_t c = kb_getchar();
		if (c < 0 && (inb(0x3FD) & 1))
			c = (int32_t)inb(0x3F8);
		if (c < 0)
			break;

		buf[n++] = (char)c;
		if (c == '\n' || c == '\r')
			break;
	}

	return (int64_t)n;
}

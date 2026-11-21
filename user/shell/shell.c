#include "syscall.h"

static size_t str_len(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

static int str_cmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

static int str_ncmp(const char *a, const char *b, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (a[i] != b[i] || a[i] == '\0')
			return (unsigned char)a[i] - (unsigned char)b[i];
	}
	return 0;
}

static void puts_stdout(const char *s)
{
	sys_write(1, s, str_len(s));
}

static void put_u32(uint32_t val)
{
	char tmp[16];
	size_t i = 0;
	if (val == 0) {
		sys_write(1, "0", 1);
		return;
	}
	while (val > 0) {
		tmp[i++] = (char)('0' + (val % 10));
		val /= 10;
	}
	while (i > 0) {
		i--;
		sys_write(1, &tmp[i], 1);
	}
}

static void cmd_ls(void)
{
	char name[16];
	uint32_t size = 0;
	for (size_t idx = 0;; idx++) {
		int64_t r = sys_readdir(idx, name, &size);
		if (r <= 0)
			break;
		puts_stdout("  ");
		puts_stdout(name);
		size_t nlen = str_len(name);
		while (nlen++ < 14)
			sys_write(1, " ", 1);
		put_u32(size);
		puts_stdout(" bytes\n");
	}
}

static void cmd_cat(const char *path)
{
	int64_t sz = sys_read_file(path, NULL, 0);
	if (sz < 0) {
		puts_stdout("File not found: ");
		puts_stdout(path);
		puts_stdout("\n");
		return;
	}
	if (sz == 0)
		return;

	char *buf = (char *)sys_alloc((size_t)sz);
	if (!buf) {
		puts_stdout("Out of memory.\n");
		return;
	}
	int64_t got = sys_read_file(path, buf, (size_t)sz);
	if (got > 0) {
		sys_write(1, buf, (size_t)got);
		if (buf[got - 1] != '\n')
			sys_write(1, "\n", 1);
	}
}

static void cmd_clear(void)
{
	sys_write(1, "\x0C", 1);
}

static size_t read_line(char *buf, size_t max_len)
{
	size_t len = 0;
	while (len + 1 < max_len) {
		char c = 0;
		int64_t r = sys_read(0, &c, 1);
		if (r <= 0)
			continue;

		if (c == '\r' || c == '\n') {
			sys_write(1, "\n", 1);
			break;
		}
		if (c == '\x0C') {
			cmd_clear();
			puts_stdout("zanux> ");
			if (len > 0)
				sys_write(1, buf, len);
			continue;
		}
		if (c == '\b' || c == 0x7F) {
			if (len > 0) {
				len--;
				sys_write(1, "\b \b", 3);
			}
			continue;
		}
		if (c >= 0x20 && c < 0x7F) {
			buf[len++] = c;
			sys_write(1, &c, 1);
		}
	}
	buf[len] = '\0';
	return len;
}

int shell_main(void)
{
	char line[128];

	puts_stdout("\nZanux Mini-Shell (type 'help' or 'doom')\n");

	for (;;) {
		puts_stdout("zanux> ");
		size_t len = read_line(line, sizeof(line));
		if (len == 0)
			continue;

		if (str_cmp(line, "help") == 0) {
			puts_stdout("Commands:\n");
			puts_stdout("  help        - show this message\n");
			puts_stdout("  ls          - list files on disk\n");
			puts_stdout("  cat <file>  - print file contents\n");
			puts_stdout("  clear       - clear screen (or Ctrl+L)\n");
			puts_stdout("  doom        - run DOOM.BIN\n");
			puts_stdout("  echo <msg>  - print text\n");
			puts_stdout("  run <file>  - spawn a binary from disk\n");
			puts_stdout("  exit        - exit shell\n");
		} else if (str_cmp(line, "ls") == 0) {
			cmd_ls();
		} else if (str_cmp(line, "clear") == 0) {
			cmd_clear();
		} else if (str_ncmp(line, "cat ", 4) == 0) {
			cmd_cat(line + 4);
		} else if (str_cmp(line, "doom") == 0) {
			puts_stdout("Launching DOOM.BIN...\n");
			if (sys_exec("DOOM.BIN") < 0)
				puts_stdout("Failed to launch DOOM.BIN.\n");
		} else if (str_ncmp(line, "echo ", 5) == 0) {
			puts_stdout(line + 5);
			puts_stdout("\n");
		} else if (str_ncmp(line, "run ", 4) == 0) {
			if (sys_exec(line + 4) < 0)
				puts_stdout("Failed to execute file.\n");
		} else if (str_cmp(line, "exit") == 0) {
			puts_stdout("Bye!\n");
			sys_exit(0);
		} else {
			puts_stdout("Unknown command: ");
			puts_stdout(line);
			puts_stdout("\n");
		}
	}
}

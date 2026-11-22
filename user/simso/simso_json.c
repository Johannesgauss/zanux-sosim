#include "simso_json.h"

struct jcur {
	const char *p;
	const char *end;
};

static void j_zero(void *dst, size_t n)
{
	uint8_t *b = (uint8_t *)dst;
	for (size_t i = 0; i < n; i++)
		b[i] = 0;
}

static bool j_streq(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return false;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static void j_skip_ws(struct jcur *c)
{
	while (c->p < c->end) {
		char ch = *c->p;
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
			c->p++;
		else
			break;
	}
}

static bool j_consume(struct jcur *c, char expected)
{
	j_skip_ws(c);
	if (c->p < c->end && *c->p == expected) {
		c->p++;
		return true;
	}
	return false;
}

static bool j_parse_string(struct jcur *c, char *buf, size_t cap)
{
	j_skip_ws(c);
	if (c->p >= c->end || *c->p != '"')
		return false;
	c->p++;

	size_t n = 0;
	while (c->p < c->end && *c->p != '"') {
		char ch = *c->p++;
		if (ch == '\\' && c->p < c->end)
			ch = *c->p++;
		if (buf && cap > 0 && n + 1 < cap)
			buf[n++] = ch;
	}
	if (c->p >= c->end || *c->p != '"')
		return false;
	c->p++;
	if (buf && cap > 0)
		buf[n] = '\0';
	return true;
}

static bool j_parse_u32(struct jcur *c, uint32_t *out)
{
	j_skip_ws(c);
	if (c->p >= c->end)
		return false;

	bool neg = false;
	if (*c->p == '-') {
		neg = true;
		c->p++;
	}
	if (c->p >= c->end || *c->p < '0' || *c->p > '9')
		return false;

	uint32_t val = 0;
	while (c->p < c->end && *c->p >= '0' && *c->p <= '9') {
		val = val * 10U + (uint32_t)(*c->p - '0');
		c->p++;
	}
	/* Skip optional fractional part if any. */
	if (c->p < c->end && *c->p == '.') {
		c->p++;
		while (c->p < c->end && *c->p >= '0' && *c->p <= '9')
			c->p++;
	}
	if (out)
		*out = neg ? 0U : val;
	return true;
}

static bool j_skip_value(struct jcur *c);

static bool j_skip_object(struct jcur *c)
{
	if (!j_consume(c, '{'))
		return false;
	if (j_consume(c, '}'))
		return true;
	for (;;) {
		if (!j_parse_string(c, NULL, 0))
			return false;
		if (!j_consume(c, ':'))
			return false;
		if (!j_skip_value(c))
			return false;
		if (j_consume(c, '}'))
			return true;
		if (!j_consume(c, ','))
			return false;
	}
}

static bool j_skip_array(struct jcur *c)
{
	if (!j_consume(c, '['))
		return false;
	if (j_consume(c, ']'))
		return true;
	for (;;) {
		if (!j_skip_value(c))
			return false;
		if (j_consume(c, ']'))
			return true;
		if (!j_consume(c, ','))
			return false;
	}
}

static bool j_skip_value(struct jcur *c)
{
	j_skip_ws(c);
	if (c->p >= c->end)
		return false;
	char ch = *c->p;
	if (ch == '"')
		return j_parse_string(c, NULL, 0);
	if (ch == '{')
		return j_skip_object(c);
	if (ch == '[')
		return j_skip_array(c);
	if (ch == '-' || (ch >= '0' && ch <= '9'))
		return j_parse_u32(c, NULL);
	/* true / false / null */
	while (c->p < c->end && *c->p >= 'a' && *c->p <= 'z')
		c->p++;
	return true;
}

static bool j_parse_task(struct jcur *c, struct task *t)
{
	if (!j_consume(c, '{'))
		return false;
	if (j_consume(c, '}'))
		return true;

	char key[32];
	for (;;) {
		if (!j_parse_string(c, key, sizeof(key)))
			return false;
		if (!j_consume(c, ':'))
			return false;

		if (j_streq(key, "id") || j_streq(key, "name")) {
			if (!j_parse_string(c, t->name, sizeof(t->name)))
				return false;
		} else if (j_streq(key, "bin") || j_streq(key, "bin_name")) {
			if (!j_parse_string(c, t->bin_name, sizeof(t->bin_name)))
				return false;
		} else if (j_streq(key, "chegada") || j_streq(key, "arrival")) {
			if (!j_parse_u32(c, &t->arrival))
				return false;
		} else if (j_streq(key, "execucao") || j_streq(key, "burst")) {
			if (!j_parse_u32(c, &t->burst))
				return false;
		} else if (j_streq(key, "deadline")) {
			if (!j_parse_u32(c, &t->deadline))
				return false;
		} else if (j_streq(key, "prioridade") || j_streq(key, "priority")) {
			if (!j_parse_u32(c, &t->priority))
				return false;
		} else if (j_streq(key, "num_paginas") || j_streq(key, "num_pages")) {
			if (!j_parse_u32(c, &t->num_pages))
				return false;
		} else {
			if (!j_skip_value(c))
				return false;
		}

		if (j_consume(c, '}'))
			return true;
		if (!j_consume(c, ','))
			return false;
	}
}

static bool j_parse_tasks(struct jcur *c, struct sim *out)
{
	if (!j_consume(c, '['))
		return false;
	if (j_consume(c, ']'))
		return true;

	for (;;) {
		if (out->n < SIM_MAX_TASKS) {
			struct task *t = &out->tasks[out->n];
			if (!j_parse_task(c, t))
				return false;
			if (t->name[0] == '\0') {
				t->name[0] = 'P';
				t->name[1] = (char)('1' + (out->n % 9));
				t->name[2] = '\0';
			}
			if (t->priority == 0)
				t->priority = 1;
			out->n++;
		} else {
			if (!j_skip_object(c))
				return false;
		}

		if (j_consume(c, ']'))
			return true;
		if (!j_consume(c, ','))
			return false;
	}
}

bool simso_parse_json(const char *json, size_t len, struct sim *out)
{
	if (!json || len == 0 || !out)
		return false;

	uint32_t saved_alg = out->alg;
	j_zero(out, sizeof(*out));
	out->alg = (saved_alg < ALG_COUNT) ? saved_alg : ALG_RR;
	out->quantum = 2;

	struct jcur c = { json, json + len };
	if (!j_consume(&c, '{'))
		return false;
	if (j_consume(&c, '}'))
		return false;

	char key[32];
	for (;;) {
		if (!j_parse_string(&c, key, sizeof(key)))
			return false;
		if (!j_consume(&c, ':'))
			return false;

		if (j_streq(key, "quantum")) {
			if (!j_parse_u32(&c, &out->quantum))
				return false;
		} else if (j_streq(key, "sobrecarga_contexto") || j_streq(key, "sobrecarga") || j_streq(key, "overhead")) {
			if (!j_parse_u32(&c, &out->overhead))
				return false;
		} else if (j_streq(key, "custo_disco") || j_streq(key, "disk_cost")) {
			if (!j_parse_u32(&c, &out->disk_cost))
				return false;
		} else if (j_streq(key, "seed")) {
			if (!j_parse_u32(&c, &out->seed))
				return false;
		} else if (j_streq(key, "processos") || j_streq(key, "tasks")) {
			if (!j_parse_tasks(&c, out))
				return false;
		} else {
			if (!j_skip_value(&c))
				return false;
		}

		if (j_consume(&c, '}'))
			break;
		if (!j_consume(&c, ','))
			return false;
	}

	return out->n > 0;
}

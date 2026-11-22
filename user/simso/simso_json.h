/**
 * @file simso_json.h
 * @brief Freestanding JSON loader for SIMSO test cases (shared by Ring 3 SIMSO.BIN and host CLI).
 */

#ifndef SIMSO_JSON_H
#define SIMSO_JSON_H

#include "sim.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Parse a JSON test case buffer into @p out.
 *
 * Supports the assignment schema:
 *   quantum, sobrecarga_contexto, custo_disco, seed,
 *   processos: [ { id, chegada, execucao, deadline, prioridade, num_paginas } ]
 *
 * Preserves @p out->alg if already set, and zeroes the rest before parsing.
 *
 * @return true if valid JSON with at least 1 process was parsed, false otherwise.
 */
bool simso_parse_json(const char *json, size_t len, struct sim *out);

#endif // SIMSO_JSON_H

CC ?= cc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
OPEN ?= xdg-open
ALG ?= RR
INPUT ?= ../casos/caso1.json
UI_ALG ?= 2

POLICY_SRCS := $(wildcard ../src/sched/policies/*.c)
SIM_CORE    := ../src/sched/sim/sim_core.c
CONTRACT    := ../src/sched/contract/sim.h
SIMSO_JSON  := ../user/simso/simso_json.c
ZGFX_SRC    := ../user/libc/zgfx.c

.PHONY: test-sched test-ui test-ui-headless run-simso view-log simso clean

test-sched: sched_host_test
	./sched_host_test

sched_host_test: sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) -o $@

test-ui-headless: ui_host_test
	./ui_host_test $(UI_ALG) out_gantt.ppm

test-ui: test-ui-headless
	@if command -v $(OPEN) >/dev/null 2>&1; then \
		$(OPEN) out_gantt.ppm >/dev/null 2>&1 & \
	else \
		echo "Abra manualmente: tools/out_gantt.ppm"; \
	fi

ui_host_test: ui_host_test.c ../user/simso/simso_render.c $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract -I ../src -idirafter ../user/libc/include \
		ui_host_test.c ../user/simso/simso_render.c $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) -o $@

simso: simso_cli.c ../user/simso/simso_render.c $(SIMSO_JSON) $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract -I ../src -I ../user/simso -idirafter ../user/libc/include \
		simso_cli.c ../user/simso/simso_render.c $(SIMSO_JSON) $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) -o $@

run-simso: simso
	./simso --alg $(ALG) --input $(INPUT) --gantt out_gantt.ppm --log out_simso.log
	@if command -v $(OPEN) >/dev/null 2>&1; then \
		$(OPEN) out_gantt.ppm >/dev/null 2>&1 & \
	else \
		echo "Abra manualmente: tools/out_gantt.ppm"; \
	fi

view-log: run-simso
	$${EDITOR:-less} out_simso.log

clean:
	rm -f sched_host_test ui_host_test simso out_gantt.ppm out_simso.log

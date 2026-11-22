CC ?= cc
CFLAGS := -O2 -Wall -Wextra -std=gnu11

POLICY_SRCS := $(wildcard ../src/sched/policies/*.c)
SIM_CORE    := ../src/sched/sim/sim_core.c
CONTRACT    := ../src/sched/contract/sim.h
SIMSO_JSON  := ../user/simso/simso_json.c
ZGFX_SRC    := ../user/libc/zgfx.c

.PHONY: test-sched test-ui simso clean

test-sched: sched_host_test
	./sched_host_test

sched_host_test: sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) -o $@

test-ui: ui_host_test
	./ui_host_test 2 out_gantt.ppm

ui_host_test: ui_host_test.c ../user/simso/simso_render.c $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract -I ../src -idirafter ../user/libc/include \
		ui_host_test.c ../user/simso/simso_render.c $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) -o $@

simso: simso_cli.c ../user/simso/simso_render.c $(SIMSO_JSON) $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract -I ../src -I ../user/simso -idirafter ../user/libc/include \
		simso_cli.c ../user/simso/simso_render.c $(SIMSO_JSON) $(ZGFX_SRC) $(SIM_CORE) $(POLICY_SRCS) -o $@

clean:
	rm -f sched_host_test ui_host_test simso out_gantt.ppm

CC ?= cc
CFLAGS := -O2 -Wall -Wextra -std=gnu11

POLICY_SRCS := $(wildcard ../src/sched/policies/*.c)
SIM_CORE    := ../src/sched/sim/sim_core.c
CONTRACT    := ../src/sched/contract/sim.h

.PHONY: test-sched test-ui clean

test-sched: sched_host_test
	./sched_host_test

sched_host_test: sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract sched_host_test.c $(SIM_CORE) $(POLICY_SRCS) -o $@

test-ui: ui_host_test
	./ui_host_test 2 out_gantt.ppm

ui_host_test: ui_host_test.c ../user/simso/simso_render.c $(SIM_CORE) $(POLICY_SRCS) $(CONTRACT)
	$(CC) $(CFLAGS) -I ../src/sched/contract -I ../src ui_host_test.c ../user/simso/simso_render.c $(SIM_CORE) $(POLICY_SRCS) -o $@

clean:
	rm -f sched_host_test ui_host_test out_gantt.ppm

#include "sim.h"

int pick_edf(struct task* t, uint32_t n, int cur, uint32_t now, uint32_t q){
        (void) now;
        (void) q;
        int best = -1;
        if (cur >= 0 && (uint32_t)cur < n && t[cur].state == TASK_RUNNING)
                best = cur;
        for (uint32_t i = 0; i < n; i++) {
                if (t[i].state == TASK_READY || t[i].state == TASK_RUNNING){

                        if (best < 0 || t[i].deadline  < t[best].deadline)
                                best = (int)i;
                }
        }
        return best;
}


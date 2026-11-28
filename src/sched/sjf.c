#include "sim.h"

int pick_sjf(struct task* t, uint32_t n, int cur, uint32_t now, uint32_t q){
	
	(void)now;
	(void)q;


	if (cur >= 0 && (uint32_t)cur < n && t[cur].state == TASK_RUNNING)
		return cur;
	
	int best = -1;

	for (uint32_t i = 0; i < n; i++){
		if (t[i].state == TASK_READY){
			if(best < 0 || t[i].burst < t[best].burst || (t[i].burst == t[best].burst && t[i].arrival < t[best].arrival))
				best = (int) i;
		}
	}
	return best;
} 

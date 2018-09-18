#include <stdio.h>
#include "ev.h"

#ifdef EV_TIMER_TEST
static void show_timer(ev_timer_t *timer)
{
	fprintf(stdout, "%s : priority %d and with interval %d s, %d us, prev %s and next %s\n", 
		timer->name, timer->priority, 
		timer->interval.seconds, timer->interval.micro_seconds, 
		(timer->prev_ev?timer->prev_ev->name:"NULL"), 
		(timer->next_ev?timer->next_ev->name:"NULL")
	);
}

static void show_timers(ev_timer_t *timer)
{
	if(!timer){
		fprintf(stdout, "no timers yet\n");
	}else{
		do{
			show_timer(timer);
			timer = timer->next_ev;
		}while(timer);
	}
}

static void test_timer()
{
	ev_loop_t ev_loop;
	ev_loop_init(&ev_loop);

	const size_t timer_num_limit = 5;
	ev_timer_t timers[timer_num_limit];
	size_t timers_num = sizeof(timers)/sizeof(timers[0]);

	size_t test_idx=0;
	for(test_idx=0; test_idx<2; ++test_idx)
	{
		printf("****************\n");
		printf("test times %d\n", test_idx+1);
		printf("****************\n");

		// 加入timer
		size_t i;
		for(i=0; i<timers_num; ++i)
		{
			ev_timer_t *timer = &timers[i];
			ev_timer_init(timer, NULL);
			int delay = rand()%20;
			ev_set_priority(timer, rand()%3);
			sprintf(timer->name, "timer%d", i+1);
			fprintf(stdout, "timer %s with priority %d and timeout after %d us\n", 
				timer->name, timer->priority, delay
			);
	
			ev_duration_t d;
			d.seconds = 0; d.micro_seconds = delay;
			ev_timer_start(&ev_loop, timer, &d);
			show_timers(ev_loop.timer_tbl);
	
			getchar();
			printf("\n");
		}
	
		// 删除timer
		size_t num = timers_num;
		while(num>0)
		{
			size_t this_time_del_idx = rand()%num;
			fprintf(stdout, "del timer with idx %d\n", this_time_del_idx);
			ev_timer_t *timer = ev_loop.timer_tbl;
			for(i=0; i<this_time_del_idx; timer=timer->next_ev, ++i);
			fprintf(stdout, "this time del timer %s\n", timer->name);
			ev_timer_stop(&ev_loop, timer);
			show_timers(ev_loop.timer_tbl);
	
			--num;
			getchar();
			printf("\n");
		}
	}
}
#endif

int main()
{
#ifdef EV_TIMER_TEST
	test_timer();
#endif
	return 0;
}


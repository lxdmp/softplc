#include "ev.h"

/*
 * event_loop
 */
void ev_loop_init(ev_loop_t *ev)
{
	ev->timer_tbl = NULL;
}

/*
 * ev_timer
 */
void ev_timer_start(ev_loop_t *ev_loop, ev_timer_t *timer, int micro_seconds)
{
#define INSERT_TIMER(timer, after_this, before_this) do{ \
	if(before_this) \
		(before_this)->interval -= (timer)->interval; \
	\
	if(after_this) \
		(after_this)->next_ev = timer; \
	(timer)->next_ev = (before_this); \
	if(before_this) \
		(before_this)->prev_ev = (timer); \
	(timer)->prev_ev = (after_this); \
}while(0) \

	timer->interval = micro_seconds;
	if(!ev_loop->timer_tbl){
		ev_loop->timer_tbl = timer;
		return;
	}
	
	ev_timer_t *after_this = NULL;
	ev_timer_t *before_this = ev_loop->timer_tbl;
	do{
		if( (timer->interval<before_this->interval) ||
			(timer->interval==before_this->interval && ev_priority_higher(timer, before_this)) )
		{
			INSERT_TIMER(timer, after_this, before_this);
			break;
		}
		timer->interval -= before_this->interval;
		after_this = before_this;
		before_this = before_this->next_ev;
	}while(before_this);
	if(!before_this)
	{
		INSERT_TIMER(timer, after_this, before_this);
	}else if(!after_this){
		ev_loop->timer_tbl = timer;
	}
#undef INSERT_TIMER
}

void ev_timer_stop(ev_loop_t *ev_loop, ev_timer_t *timer)
{
	if(timer->next_ev){
		timer->next_ev->interval += timer->interval;
		timer->next_ev->prev_ev = timer->prev_ev;
	}
	if(timer->prev_ev){
		timer->prev_ev->next_ev = timer->next_ev;
	}
	timer->prev_ev = NULL;
	timer->next_ev = NULL;
}

#ifdef EV_TIMER_TEST
#include <stdio.h>
static void show_timers(ev_timer_t *timer)
{
	if(!timer)
		fprintf(stdout, "no timers yet\n");
	do{
		fprintf(stdout, "%s : priority %d and with interval %d us, prev %s and next %s\n", 
			timer->name, timer->priority, timer->interval, 
			(timer->prev_ev?timer->prev_ev->name:"NULL"), 
			(timer->next_ev?timer->next_ev->name:"NULL")
		);
		timer = timer->next_ev;
	}while(timer);
}

static void test_timer()
{
	ev_loop_t ev_loop;
	ev_loop_init(&ev_loop);

	ev_timer_t timers[20];
	size_t i;
	for(i=0; i<sizeof(timers)/sizeof(timers[0]); ++i)
	{
		ev_timer_t *timer = &timers[i];
		ev_timer_init(timer, NULL);
		int delay = rand()%20;
		ev_set_priority(timer, rand()%3);
		sprintf(timer->name, "timer%d", i+1);
		fprintf(stdout, "timer %s with priority %d and timeout after %d us\n", 
			timer->name, timer->priority, delay
		);

		ev_timer_start(&ev_loop, timer, delay);
		show_timers(ev_loop.timer_tbl);

		getchar();
		printf("\n");
	}
}

int main()
{
	test_timer();
	return 0;
}
#endif


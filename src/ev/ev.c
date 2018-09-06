#include "ev.h"
#include "port.h"

/*
 * 该事件循环实现中,不动态分配内存,按照一定顺序升序(anfds),
 * 需要进行一些二分查找,故编写一组宏.
 */
#define BINARY_SEARCH(_buf, _lower, _upper, _searcher, _target, _cmp_func) do{ \
	int32_t index; \
	while(_lower<_upper) \
	{ \
		index = (_lower+_upper)>>1; \
		_searcher = &((_buf)[index]); \
		int cmp_ret = _cmp_func(_searcher, _target); \
		if(cmp_ret<0) \
			_lower = index+1; \
		else if(cmp_ret>0) \
			_upper = index; \
		else \
			break; \
	} \
}while(0)

#define search_func_between_anfds_and_ev_io(anfd, ev_io) \
	((anfd->fd<ev_io->fd)?(-1): \
		(anfd->fd>ev_io->fd)?(1):(0))

#define search_func_between_anfds_and_fd(anfd, fd_) \
	((anfd->fd<fd_)?(-1): \
		(anfd->fd>fd_)?(1):(0))

/*************
 * event_loop
 *************/
void ev_loop_init(ev_loop_t *ev_loop)
{
	ev_loop->anfd_cnt = 0;
	ev_loop->timer_tbl = NULL;
	int priority_idx=0;
	for(;priority_idx<EV_PRIORITY_NUM; ++priority_idx)
		ev_loop->pending_cnt[priority_idx] = 0;
	install_backend_impl(ev_loop);
}

// 检测io事件的变化
static void check_ev_io_modification(ev_loop_t *ev_loop)
{
	int32_t i;
	for(i = 0; i<ev_loop->anfd_cnt; ++i)
	{
		ANFD *anfd = &(ev_loop->anfds[i]);
		ev_io_t *ev_io = NULL;
		if(anfd->refresh) // 该anfd关联的ev_io发生了更新.
		{
			anfd->refresh = 0;
			// 该fd关注的事件前后是否发生了变化
			int32_t old_events_focused = anfd->events_focused;
			anfd->events_focused = EV_NONE;
			for(ev_io=(ev_io_t*)anfd->head; ev_io; ev_io = ev_io->next_ev)
				anfd->events_focused |= ev_io->events_focused;
			if(old_events_focused != anfd->events_focused)
				ev_loop->backend_modify(ev_loop, anfd->fd, old_events_focused, anfd->events_focused);
		}
    }
}

void ev_io_event(ev_loop_t *ev_loop, fd_type_t fd, int32_t events)
{
	ANFD *current = NULL;
	int32_t lower = 0, upper = ev_loop->anfd_cnt;
	BINARY_SEARCH(&(ev_loop->anfds[0]), lower, upper, current, fd, search_func_between_anfds_and_fd);
	if(!(current && current->fd==fd))
		FATAL_ERROR("ev_io_event with fd %d, but this fd not in anfds.\n", fd);

	if(current->events_focused & events)
	{
		ev_io_t *ev_io = current->head;
		while(ev_io)
		{
			int32_t event_occur = ev_io->events_focused & events;
			if(event_occur)
			{
				// 将该ev_io加入到pendings中,注意仍然保存在anfds中.
				int32_t ev_priority = ev_io->priority;
				if(ev_loop->pending_cnt[ev_priority]>=EV_PRIORITY_PENDING_NUM)
					FATAL_ERROR("fd %d with priority %d and event 0x%x occur, and exceeds EV_PRIORITY_PENDING_NUM which is %d", fd, ev_io->priority, event_occur, EV_PRIORITY_PENDING_NUM);
				ANPENDING *anpending = &(ev_loop->pendings[ev_priority][ev_loop->pending_cnt[ev_priority]]);
				anpending->ev = ((ev_base_t*)(void*)(ev_io));
				anpending->event_occur = event_occur;
				++ev_loop->pending_cnt[ev_priority];
			}
			ev_io = ev_io->next_ev;
		}
	}
}


static void ev_timer_event(ev_loop_t *ev_loop)
{
}

void ev_loop_run(ev_loop_t *ev_loop)
{
	// 检测ev_io的变化
	check_ev_io_modification(ev_loop);

	// 获取下次要等待的时间
	ev_duration_t *block_duration_ptr = NULL, block_duration;
	if(ev_loop->timer_tbl)
	{
		memcpy(&block_duration, &ev_loop->timer_tbl->interval, sizeof(ev_duration_t));
		block_duration_ptr = &block_duration;
	}

	// 等待事件发生
	ev_duration_t entry_block,leave_block;
	get_boot_duration(&entry_block);
	ev_loop->backend_poll(ev_loop, block_duration_ptr);
	get_boot_duration(&leave_block);
	
	// ...
}

/***********
 * ev_timer
 ***********/
void ev_timer_start(ev_loop_t *ev_loop, ev_timer_t *timer, ev_duration_t *interval)
{
#define INSERT_TIMER(timer, after_this, before_this) do{ \
	if(before_this) \
		ev_duration_sub((before_this)->interval, (timer)->interval); \
	if(after_this) \
		(after_this)->next_ev = timer; \
	(timer)->next_ev = (before_this); \
	if(before_this) \
		(before_this)->prev_ev = (timer); \
	(timer)->prev_ev = (after_this); \
}while(0) \

	// active
	if(ev_is_active(timer))
		return;

	// not-active
	ev_activate(timer);

	memcpy(&timer->interval, interval, sizeof(ev_duration_t));
	if(!ev_loop->timer_tbl){
		ev_loop->timer_tbl = timer;
		return;
	}
	
	ev_timer_t *after_this = NULL;
	ev_timer_t *before_this = ev_loop->timer_tbl;
	do{
		
		if( (ev_duration_lt(timer->interval, before_this->interval)) ||
			(ev_duration_eq(timer->interval, before_this->interval) && ev_priority_higher_than(timer, before_this)) )
		{
			INSERT_TIMER(timer, after_this, before_this);
			break;
		}
		ev_duration_sub(timer->interval, before_this->interval);
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
	// inactive
	if(ev_is_inactive(timer))
		return;

	// active
	if(!ev_is_pending(timer))
	{
		// not pending
		if(timer->next_ev){
			ev_duration_add(timer->next_ev->interval, timer->interval);
			timer->next_ev->prev_ev = timer->prev_ev;
		}
		if(timer->prev_ev){
			timer->prev_ev->next_ev = timer->next_ev;
		}else{
			ev_loop->timer_tbl = timer->next_ev;
		}
		timer->prev_ev = NULL;
		timer->next_ev = NULL;
	}else{
		// pending
		// TODO pending contains index of ev in pending-ev-buf.
		ev_pending_reset(timer);
	}
	ev_inactivate(timer);
}

#ifdef EV_TIMER_TEST
#include <stdio.h>
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

int main()
{
	test_timer();
	return 0;
}
#endif

/********
 * ev_io
 ********/
void ev_io_start(ev_loop_t *ev_loop, ev_io_t *ev_io)
{
	// already active
	if(ev_is_active(ev_io))
		return;

	// not active yet
	ev_activate(ev_io);

	// 更新到ev_loop_t的anfds中,anfds按照fd的升序排列.
	ANFD *current = NULL;
	int32_t lower = 0, upper = ev_loop->anfd_cnt;
	BINARY_SEARCH(&(ev_loop->anfds[0]), lower, upper, current, ev_io, search_func_between_anfds_and_ev_io);

	if(current && current->fd==ev_io->fd)
	{
		// 更新到current
		current->refresh = 1;
		ev_io->next_ev = current->head; // 头部插入
		ev_io->prev_ev = NULL;
		if(current->head)
			current->head->prev_ev = ev_io;
		current->head = ev_io;
	}else{
		// 插入到lower位置
		if(ev_loop->anfd_cnt>=MAX_FD_NUMS)
			FATAL_ERROR("exceed fd num limit which is %d.\n", MAX_FD_NUMS);
		memmove(&(ev_loop->anfds[lower+1]), &(ev_loop->anfds[lower]), sizeof(ANFD)*(ev_loop->anfd_cnt-lower));
		++ev_loop->anfd_cnt;
		current = &(ev_loop->anfds[lower]);
		current->fd = ev_io->fd;
		current->refresh = 1;
		current->head = ev_io;
	}
}

void ev_io_stop(ev_loop_t *ev_loop, ev_io_t *ev_io)
{
	// inactive
	if(ev_is_inactive(ev_io))
		return;

	// active
	if(ev_is_pending(ev_io))
	{
		// pending
		// TODO pending contains index of ev in pending-ev-buf.
		ev_pending_reset(ev_io);
	}

	// 更新所在的anfd.
	ANFD *current = NULL;
	int32_t lower = 0, upper = ev_loop->anfd_cnt;
	BINARY_SEARCH(&(ev_loop->anfds[0]), lower, upper, current, ev_io, search_func_between_anfds_and_ev_io);

	if(!(current && current->fd==ev_io->fd))
		FATAL_ERROR("internal logic error, ev_io active but not in anfds.\n");

	// 设置刷新标记并从双链表中移除
	current->refresh = 1;
	if(ev_io->next_ev)
		ev_io->next_ev->prev_ev = ev_io->prev_ev;
	if(ev_io->prev_ev)
		ev_io->prev_ev->next_ev = ev_io->next_ev;
	else
		current->head = ev_io->next_ev;
	ev_io->prev_ev = NULL;
	ev_io->next_ev = NULL;

	ev_inactivate(ev_io);
}

/*************
 * ev_prepare
 *************/

// 就绪事件的默认回调
static void dummy_pending_ev_cb(ev_loop_t *ev_loop, ev_prepare_t *ev, int events)
{
}


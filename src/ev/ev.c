#include "ev.h"
#include "port.h"

/*
 * pendings维护已发生但尚未触发的事件,按照优先级维护,
 * 相同优先级的,每个io事件单独占一个位置,所有定时事件占一个位置,按照事件进行排列.
 *
 * 事件的pending记录了在该优先级下的位置.
 */

/*
 * 该事件循环实现中,不动态分配内存,按照一定顺序升序(anfds/pendings),
 * 需要进行一些二分查找,故编写一组宏来实现泛型.
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

#define search_func_between_anpendings_and_events(anpending, events) \
	((anpending->event_occur<events)?(-1): \
		(anpending->event_occur>events)?(1):(0))

/*************
 * event_loop
 *************/
void ev_loop_init(ev_loop_t *ev_loop)
{
	ev_loop->anfd_cnt = 0;
	ev_loop->timer_tbl = NULL;
	int priority_idx=0;
	for(;priority_idx<EV_PRIORITY_NUM; ++priority_idx)
		ev_loop->anpending_cnt[priority_idx] = 0;
	install_backend_impl(ev_loop);
}

// anpending成员的操作
static void ev_loop_pending_set_io(ev_loop_t *ev_loop, ev_io_t *ev_io, int event_occur)
{
	if(ev_is_inactive(ev_io))
		return;

	if(ev_is_pending(ev_io))
		return;

	int32_t ev_priority = ev_io->priority;
	if(ev_loop->anpending_cnt[ev_priority]>=EV_PRIORITY_PENDING_NUM)
		FATAL_ERROR("fd %d with priority %d and event 0x%x occur, and exceeds EV_PRIORITY_PENDING_NUM which is %d", ev_io->fd, ev_io->priority, event_occur, EV_PRIORITY_PENDING_NUM);

	ANPENDING *anpending = NULL;
	int32_t lower = 0, upper = ev_loop->anpending_cnt[ev_priority];
	ANPENDING *base = &(ev_loop->anpendings[ev_priority][0]);
	BINARY_SEARCH(base, lower, upper, 
		anpending, event_occur, search_func_between_anpendings_and_events
	);
	if(!anpending)
		FATAL_ERROR("ev_io_event with fd %d, but no position in anpendings.\n", ev_io->fd);

	// 加入到lower位置
	memmove(&(base[lower+1]), &(base[lower]), sizeof(ANPENDING)*(ev_loop->anpending_cnt[ev_priority]-lower));
	anpending = &(base[lower]);
	anpending->ev = (ev_list_t*)(void*)(ev_io);
	anpending->event_occur = event_occur;
	ev_io->pending = lower;
	++ev_loop->anpending_cnt[ev_priority];
}

static void ev_loop_pending_unset_io(ev_loop_t *ev_loop, ev_io_t *ev_io)
{
	if(ev_is_inactive(ev_io))
		return;

	if(ev_is_not_pending(ev_io))
		return;

	if(ev_io->pending>=ev_loop->anpending_cnt[ev_io->priority])
		FATAL_ERROR("internal logic error, ev_io_event pending %d, exceed max-pending-num %d in this priority.\n", ev_io->pending, ev_loop->anpending_cnt[ev_io->priority]);

	ANPENDING *base = &(ev_loop->anpendings[ev_io->priority][0]);
	int32_t position = ev_io->pending;
	int32_t max_position_this_priority = ev_loop->anpending_cnt[ev_io->priority]-1;
	if(position<max_position_this_priority)
		memmove(&base[position], &base[position+1], sizeof(ANPENDING)*(max_position_this_priority-position));
	--ev_loop->anpending_cnt[ev_io->priority];
}

static void ev_loop_pending_set_timer(ev_loop_t *ev_loop, ev_timer_t *ev_timer)
{
	if(ev_is_inactive(ev_timer))
		return;

	if(ev_is_pending(ev_timer))
		return;

	int32_t ev_priority = ev_timer->priority;
	if(ev_loop->anpending_cnt[ev_priority]>=EV_PRIORITY_PENDING_NUM)
		FATAL_ERROR("ev_timer_event with priority %d occur, and exceeds EV_PRIORITY_PENDING_NUM which is %d", ev_timer->priority, EV_PRIORITY_PENDING_NUM);

	ANPENDING *anpending = NULL;
	int32_t lower = 0, upper = ev_loop->anpending_cnt[ev_priority];
	ANPENDING *base = &(ev_loop->anpendings[ev_priority][0]);
	BINARY_SEARCH(base, lower, upper, 
		anpending, EV_TIMEOUT, search_func_between_anpendings_and_events
	);
	if(!anpending)
		FATAL_ERROR("ev_timer_event without position in anpendings.\n");

	// 加入到lower位置
	if(base[lower].event_occur==EV_TIMEOUT)
	{
		anpending = &(base[lower]);
		if(!anpending->ev)
			FATAL_ERROR("internal logic error, anpendings[%d][%d] is occupied by EV_TIMEOUT, but no ev_timer_event.\n", ev_timer->priority, lower);
		
		ev_timer_t *inserted_timer = (ev_timer_t*)(anpending->ev);
		while(inserted_timer->next_ev) // 尾插入
			inserted_timer = inserted_timer->next_ev;
		inserted_timer->next_ev = ev_timer;
		ev_timer->next_ev = NULL;
		ev_timer->prev_ev = inserted_timer;

		ev_timer->pending = lower;
	}else{
		memmove(&(base[lower+1]), &(base[lower]), sizeof(ANPENDING)*(ev_loop->anpending_cnt[ev_priority]-lower));
		anpending = &(base[lower]);
		anpending->ev = (ev_list_t*)(void*)(ev_timer);
		anpending->event_occur = EV_TIMEOUT;

		ev_timer->pending = lower;
		++ev_loop->anpending_cnt[ev_priority];
	}
}

static void ev_loop_pending_unset_timer(ev_loop_t *ev_loop, ev_timer_t *ev_timer)
{
	if(ev_is_inactive(ev_timer))
		return;

	if(ev_is_not_pending(ev_timer))
		return;

	if(ev_timer->pending>=ev_loop->anpending_cnt[ev_timer->priority])
		FATAL_ERROR("internal logic error, ev_timer_event pending %d, exceed max-pending-num %d in this priority.\n", ev_timer->pending, ev_loop->anpending_cnt[ev_timer->priority]);

	ANPENDING *base = &(ev_loop->anpendings[ev_timer->priority][0]);
	int32_t position = ev_timer->pending;
	if(ev_timer->next_ev)
		ev_timer->next_ev->prev_ev = ev_timer->prev_ev;
	if(ev_timer->prev_ev)
		ev_timer->prev_ev->next_ev = ev_timer->next_ev;
	else
		base[position].ev = (ev_list_t*)(ev_timer->next_ev);
	ev_timer->prev_ev = NULL;
	ev_timer->next_ev = NULL;
	if(!base[position].ev)
	{
		int32_t max_position_this_priority = ev_loop->anpending_cnt[ev_timer->priority]-1;
		memmove(&base[position], &base[position+1], sizeof(ANPENDING)*(max_position_this_priority-position));
		--ev_loop->anpending_cnt[ev_timer->priority];
	}
}

// 检测io事件的变化
static void check_ev_io_modification(ev_loop_t *ev_loop)
{
	int32_t i;
	for(i = 0; i<ev_loop->anfd_cnt; ++i)
	{
		ANFD *anfd = &(ev_loop->anfds[i]);
		if(anfd->refresh) // 该anfd关联的ev_io发生了更新.
		{
			anfd->refresh = 0;
			// 该fd关注的事件前后是否发生了变化
			int32_t old_events_focused = anfd->events_focused;
			anfd->events_focused = EV_NONE;
			ev_io_t *ev_io = NULL;
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
				ev_loop_pending_set_io(ev_loop, ev_io, event_occur);
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

	// already active state
	if(ev_is_active(timer))
		return;

	// 加入到定时器链表中
	memcpy(&timer->interval, interval, sizeof(ev_duration_t));
	if(!ev_loop->timer_tbl){
		ev_loop->timer_tbl = timer;
	}else{
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
		
		if(!before_this) // 尾插入
			INSERT_TIMER(timer, after_this, before_this);
		else if(!after_this) // 首插入
			ev_loop->timer_tbl = timer;
	}

	// activate timer
	ev_activate(timer);

#undef INSERT_TIMER
}

void ev_timer_stop(ev_loop_t *ev_loop, ev_timer_t *timer)
{
	// already inactive state
	if(ev_is_inactive(timer))
		return;

	// active state
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
		ev_loop_pending_unset_timer(ev_loop, timer);
		ev_pending_reset(timer);
	}

	// inactivate timer
	ev_inactivate(timer);
}

/********
 * ev_io
 ********/
void ev_io_start(ev_loop_t *ev_loop, ev_io_t *ev_io)
{
	// already active
	if(ev_is_active(ev_io))
		return;

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

	// activate
	ev_activate(ev_io);
}

void ev_io_stop(ev_loop_t *ev_loop, ev_io_t *ev_io)
{
	// inactive
	if(ev_is_inactive(ev_io))
		return;

	// pending
	if(ev_is_pending(ev_io))
	{
		// pending
		ev_loop_pending_unset_io(ev_loop, ev_io);
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

	// inactivate
	ev_inactivate(ev_io);
}

/*************
 * ev_prepare
 *************/

// 就绪事件的默认回调
static void dummy_pending_ev_cb(ev_loop_t *ev_loop, ev_prepare_t *ev, int events)
{
}


#ifndef _EV_H_
#define _EV_H_

#include "../platform.h"

struct ev_loop_t;

/*
 * 各类事件掩码
 */
enum ev_mask_t{
	EV_NONE = 0x00, 
	EV_READABLE = 0x01, // 可读
	EV_WRITABLE = 0x02, // 可写
	EV_RW = 0x03, // 可读写
	EV_TIMEOUT = 0x04 // 定时
};

/*
 * ev_base : 基本事件.
 *
 * active : 事件是否使能;
 * pending : 事件是否就绪;
 * priority : 事件优先级;
 * cb : 回调;
 * data : 自定义数据.
 */
#define EV_BASE(ev_type_t) \
	int8_t name[32]; \
	int32_t active; \
	int32_t pending; \
	int32_t priority; \
	void (*cb)(struct ev_loop_t *ev_loop, struct ev_type_t *ev, int revents); \
	void *data; \

typedef struct ev_base_t{
	EV_BASE(ev_base_t);
}ev_base_t;

// active状态
#define EV_INACTIVE 0
#define EV_ACTIVE 1

#define ev_is_active(ev) \
	(((ev_base_t*)(void*)(ev))->active==EV_ACTIVE)

#define ev_is_inactive(ev) (!ev_is_active(ev))

#define ev_activate(ev) do{ \
	((ev_base_t*)(void*)(ev))->active = EV_ACTIVE; \
}while(0)

#define ev_inactivate(ev) do{ \
	((ev_base_t*)(void*)(ev))->active = EV_INACTIVE; \
}while(0)

// pending状态
#define EV_NOT_PENDING 0
#define EV_PENDING 1

#define ev_is_pending(ev) \
	(((ev_base_t*)(void*)(ev))->pending==EV_PENDING)

#define ev_is_not_pending(ev) \
	(((ev_base_t*)(void*)(ev))->pending==EV_NOT_PENDING)

#define ev_pending_set(ev) do{ \
	((ev_base_t*)(void*)(ev))->pending = EV_PENDING; \
}while(0)

#define ev_pending_reset(ev) do{ \
	((ev_base_t*)(void*)(ev))->pending = EV_NOT_PENDING; \
}while(0)

// priority状态
#define EV_HIGH_PRIORITY -3 
#define EV_LOW_PRIORITY 3
#define EV_DEFAULT_PRIORITY 0
#define EV_PRIORITY_NUM (EV_LOW_PRIORITY-EV_HIGH_PRIORITY+1)
#define EV_PRIORITY_PENDING_NUM (MAX_FD_NUMS<<1)

#define ev_priority_higher_than(ev1, ev2) \
	(((ev_base_t*)(void*)(ev1))->priority < ((ev_base_t*)(void*)(ev2))->priority) \

#define ev_priority_lower_than(ev1, ev2) \
	(((ev_base_t*)(void*)(ev1))->priority > ((ev_base_t*)(void*)(ev2))->priority) \

#define ev_priority_eq_to(ev1, ev2) \
	(((ev_base_t*)(void*)(ev1))->priority == ((ev_base_t*)(void*)(ev2))->priority) \

#define ev_set_priority(ev, pri) do{ \
	if(ev_is_inactive(ev)) \
	{ \
		((ev_base_t*)(void*)(ev))->priority = (pri); \
		if(((ev_base_t*)(void*)(ev))->priority<EV_HIGH_PRIORITY) \
			((ev_base_t*)(void*)(ev))->priority = EV_HIGH_PRIORITY; \
		else if(((ev_base_t*)(void*)(ev))->priority>EV_LOW_PRIORITY) \
			((ev_base_t*)(void*)(ev))->priority= EV_LOW_PRIORITY; \
	} \
}while(0) \

// cb状态
#define ev_set_cb(ev, cb_) do{ \
	((ev_base_t*)(void*)(ev))->cb = (cb_); \
}while(0) \

#define ev_init(ev, cb) do{ \
	((ev_base_t*)(void*)(ev))->name[0] = '\0'; \
	((ev_base_t*)(void*)(ev))->active = EV_INACTIVE; \
	ev_pending_reset((ev)); \
	ev_set_priority((ev), EV_DEFAULT_PRIORITY); \
	ev_set_cb ((ev), cb); \
}while(0) \

/*
 * ev_list : 以链表组织的事件(继承于ev_base).
 *
 * prev/next_ev : 前一个/后一个事件.
 */
#define EV_LIST(ev_type_t) \
	EV_BASE(ev_type_t) \
	struct ev_type_t *prev_ev; \
	struct ev_type_t *next_ev; \

typedef struct ev_list_t{
	EV_LIST(ev_list_t);
}ev_list_t;

#define ev_list_init(ev, cb) do{ \
	ev_init(ev, cb); \
	((ev_list_t*)(void*)(ev))->prev_ev = NULL; \
	((ev_list_t*)(void*)(ev))->next_ev = NULL; \
}while(0) \

/*
 * ev_timer : 定时事件(所有定时都为oneshot).
 */
typedef struct ev_duration_t{
	int32_t seconds;
	int32_t micro_seconds;
}ev_duration_t;

#define ev_duration_lt(a, b) \
	( \
		((a).seconds<(b).seconds) || \
		(((a).seconds==(b).seconds) && ((a).micro_seconds<(b).micro_seconds)) \
	) \

#define ev_duration_eq(a, b) \
	(((a).seconds==(b).seconds) && ((a).micro_seconds==(b).micro_seconds)) \

#define ev_duration_le(a, b) \
	(ev_duration_lt(a, b) || ev_duration_eq(a, b)) \

#define MICRO_SECONDS_ONE_SECOND 1000000

#define ev_duration_add(a, b) do{ \
	a.seconds += b.seconds; \
	a.micro_seconds += b.micro_seconds; \
	if(a.micro_seconds>MICRO_SECONDS_ONE_SECOND){ \
		a.seconds += 1; \
		a.micro_seconds %= MICRO_SECONDS_ONE_SECOND; \
	} \
}while(0) \

#define ev_duration_sub(a, b) do{ \
	a.seconds -= b.seconds; \
	a.micro_seconds -= b.micro_seconds; \
	if(a.micro_seconds<0){ \
		a.seconds -= 1; \
		a.micro_seconds += MICRO_SECONDS_ONE_SECOND; \
	} \
}while(0) \

typedef struct ev_timer_t{
	EV_LIST(ev_timer_t)
	ev_duration_t interval; 
}ev_timer_t;

#define ev_timer_init(ev, cb) do{ \
	ev_list_init(ev, cb); \
	((ev_timer_t*)(void*)(ev))->interval.seconds = 0; \
	((ev_timer_t*)(void*)(ev))->interval.micro_seconds = 0; \
}while(0) \

void ev_timer_start(struct ev_loop_t *ev_loop, ev_timer_t *timer, ev_duration_t *interval);
void ev_timer_stop(struct ev_loop_t *ev_loop, ev_timer_t *timer);

/*
 * ev_io : io事件.
 */
typedef struct ev_io_t{
	EV_LIST(ev_io_t);
	fd_type_t fd;
	int32_t events_focused;
}ev_io_t;

#define ev_io_set(ev, fd_, events_focused_) do{  \
	(ev)->fd = (fd_); \
	(ev)->events_focused = (events_focused_)&EV_RW; \
}while(0) \

#define ev_io_init(ev, cb, fd, events_focused) do{ \
	ev_init((ev), (cb)); \
	ev_io_set((ev), (fd), (events_focused)); \
}while(0) \

void ev_io_start(struct ev_loop_t *ev_loop, ev_io_t *ev_io);
void ev_io_stop(struct ev_loop_t *ev_loop, ev_io_t *ev_io);

/*
 * ev_prepare : prepare事件(一次事件循环阻塞前)
 */
#define EV_PREPARE(ev_type_t) \
	EV_BASE(ev_type_t) \

typedef struct ev_prepare_t{
	EV_PREPARE(ev_prepare_t);
}ev_prepare_t;

/*
 * ev_check : check事件(一次时间循环阻塞后)
 */
#define EV_CHECK(ev_type_t) \
	EV_BASE(ev_type_t) \

typedef struct ev_check_t{
	EV_CHECK(ev_check_t);
}ev_check_t;

/*
 * ev_loop : 事件循环.
 */
typedef struct ANFD
{
	fd_type_t fd; // 文件描述符
	struct ev_io_t *head; // 相关的io事件
	int32_t events_focused; // 该描述符关心的事件
	int32_t refresh; // 该描述符上注册的事件(head上)是否发生变化
}ANFD; // io事件维护结构

typedef struct ANPENDING
{
	struct ev_base_t *ev; // 相关的事件
	int32_t event_occur; // 发生的事件
}ANPENDING; // 已就绪事件维护结构

typedef struct ev_loop_t{
	struct ANFD anfds[MAX_FD_NUMS]; // io事件(按fd顺序排列)
	int32_t anfd_cnt;
	struct ev_timer_t *timer_tbl; // timer事件
	struct ANPENDING pendings[EV_PRIORITY_NUM][EV_PRIORITY_PENDING_NUM]; // 已就绪的事件
	int32_t pending_cnt[EV_PRIORITY_NUM];
	void (*backend_modify)(struct ev_loop_t*, fd_type_t, int32_t, int32_t); // reactor实现
	void (*backend_poll)(struct ev_loop_t*, struct ev_duration_t*);
}ev_loop_t;

void ev_loop_init(ev_loop_t *ev_loop);
void ev_loop_run(ev_loop_t *ev_loop);

#endif


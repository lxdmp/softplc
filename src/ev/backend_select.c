#include "ev.h"

#define USE_BACKEND_SELECT
//#undef USE_BACKEND_SELECT

#ifdef USE_BACKEND_SELECT
static void backend_select_modify(
	struct ev_loop_t *ev_loop, fd_type_t fd, 
	int32_t old_events, int32_t new_events
)
{
}

static void backend_select_poll(struct ev_loop_t *ev_loop, struct ev_duration_t *timeout)
{
}

void install_backend_impl(ev_loop_t *ev_loop)
{
	ev_loop->backend_modify = backend_select_modify;
	ev_loop->backend_poll = backend_select_poll;
}
#endif


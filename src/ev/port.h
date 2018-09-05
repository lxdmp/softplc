#ifndef _PORT_H_
#define _PORT_H_

#include "ev.h"

extern void install_backend_impl(ev_loop_t *ev_loop);
extern void get_boot_duration(ev_duration_t *duration);

#endif


#include "ev.h"

#ifdef __linux__
#include <time.h>
#include <errno.h>
void get_boot_duration(ev_duration_t *duration)
{
	struct timespec val;
	if(clock_gettime(CLOCK_MONOTONIC, &val))
		FATAL_ERROR("failed to get actual time, errno %d\n", errno);
	duration->seconds = val.tv_sec;
	duration->micro_seconds = val.tv_nsec/1000;
}
#endif


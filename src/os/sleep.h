/** @file src/os/sleep.h OS-independent inclusion of the delay routine. */

#ifndef OS_SLEEP_H
#define OS_SLEEP_H

#if defined(_WIN32)
	#include <windows.h>
	#define sleep(x) Sleep(x * 1000)
	#define msleep(x) Sleep(x)
#elif defined(__3DS__)
	#include <3ds.h>
	#define sleep(x) svcSleepThread((u64)(x) * 1000000000ULL)
	#define msleep(x) svcSleepThread((u64)(x) * 1000000ULL)
#else
	#if !defined(__USE_BSD)
		#define __USE_BSD
		#include <unistd.h>
		#undef __USE_BSD
	#else
		#include <unistd.h>
	#endif /* __USE_BSD */

	#define msleep(x) usleep(x * 1000)
#endif /* _WIN32 */

#if defined(_WIN32) && !defined(WITH_SDL) && !defined(WITH_SDL2)
#define sleepIdle() msleep(1)
#else /* _WIN32 */
#include "../timer.h"
#define sleepIdle SleepAndProcessBackgroundTasks
#endif /* _WIN32 */

#endif /* OS_SLEEP_H */

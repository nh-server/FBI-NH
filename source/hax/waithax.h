#pragma once

#include <3ds/types.h>

#define KSEMAPHORE_SIZE		  (0x2C)
#define KSEMAPHORE_VTABLESIZE (0x16 * sizeof(void*))
typedef struct KSemaphore {
	void **vtable;
	u32 refCount;
	u32 __a;
	u32 __b;
	u32 __c;
	u32 __ievent[3];
	u32 count;
	u32 maxCount;
	void *owner;
} __attribute__((packed)) KSemaphore;


bool waithax_run(void);
void waithax_cleanup(void);
void waithax_debug(bool enabled);
void waithax_backdoor(void (*method)(void));

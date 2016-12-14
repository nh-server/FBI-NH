.arm
.section .text


@ Shamelessly based from Steveice's memchunkhax2 repo. I miss those old days
@ Credits to TuxSH for finding this leak
@ Please don't expect KTM for this
.global svcCreateSemaphoreKAddr
.type 	svcCreateSemaphoreKAddr, %function
svcCreateSemaphoreKAddr:
	str r0, [sp, #-4]!
	str r3, [sp, #-4]!
	svc 0x15
	ldr r3, [sp], #4
	sub r2, r2, #4								@ Fix the kobject ptr
	str r2, [r3]
	ldr r3, [sp], #4
	str r1, [r3]
	bx lr

.global svcGetHandleInfo
.type   svcGetHandleInfo, %function
svcGetHandleInfo:
	str r0, [sp, #-0x4]!
	svc 0x29
	ldr r3, [sp], #4
	str r1, [r3]
	str r2, [r3, #4]
	bx lr


@ Here for debug/dev purposes
.global svc_7b
.type   svc_7b, %function
svc_7b:
    push {r0, r1, r2}
    mov r3, sp
    add r0, pc, #12
    svc 0x7b
    add sp, sp, #8
    ldr r0, [sp], #4
    bx lr
    cpsid aif
    ldr r2, [r3], #4
    ldmfd r3!, {r0, r1}
    push {r3, lr}
    blx r2
    pop {r3, lr}
    str r0, [r3, #-4]!
    mov r0, #0
    bx lr

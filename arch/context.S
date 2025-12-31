    .cpu    cortex-m3
    .fpu    softvfp
    .syntax unified
    .thumb
    .text

    .global trigger_pend_sv
    .global start_first_task
    .global PendSV_Handler

    /* 导入 C 变量 */
    .global current_task
    .global next_task

/* 触发 PendSV */
.thumb_func
trigger_pend_sv:
    LDR     R0, =0xE000ED04
    LDR     R1, =0x10000000
    STR     R1, [R0]
    BX      LR

/* 启动第一个任务 */
.thumb_func
start_first_task:
    /* 获取当前任务的栈指针 */
    LDR     R0, =current_task
    LDR     R0, [R0]          /* R0 = task ptr */
    LDR     R0, [R0]          /* R0 = task->sp */
    
    /* 从任务栈恢复 R4-R11 (软件保存的寄存器) */
    LDMIA   R0!, {R4-R11}
    
    /* R0 现在指向硬件栈帧 (R0-R3, R12, LR, PC, xPSR) */
    /* 设置 PSP 指向硬件栈帧 */
    MSR     PSP, R0
    ISB
    
    /* 触发 SVC 异常，在 Handler 模式下进行异常返回 */
    SVC     0
    
    /* 不应该执行到这里 */
    B       .

/* SVC Handler: 用于启动第一个任务 */
.thumb_func
.global SVC_Handler
SVC_Handler:
    /* 设置 CONTROL 寄存器：使用 PSP */
    MRS     R0, CONTROL
    ORR     R0, R0, #2
    MSR     CONTROL, R0
    
    /* 开启全局中断 */
    CPSIE   I
    
    /* 使用异常返回，硬件会自动从 PSP 恢复 R0-R3, R12, LR, PC, xPSR */
    LDR     LR, =0xFFFFFFFD   /* 返回线程模式，使用 PSP */
    BX      LR

/* 上下文切换 */
.thumb_func
PendSV_Handler:
    MRS     R2, PSP
    STMDB   R2!, {R4-R11}
    
    LDR     R0, =current_task
    LDR     R1, [R0]
    STR     R2, [R1]          /* Save SP */
    
    LDR     R0, =next_task    /* Load Next */
    LDR     R1, [R0]
    LDR     R2, [R1]
    
    LDMIA   R2!, {R4-R11}
    MSR     PSP, R2
    
    LDR     R0, =current_task
    STR     R1, [R0]          /* Update current */
    
    BX      LR

    .syntax unified
    .cpu cortex-m3
    .fpu softvfp
    .thumb

    .global g_pfnVectors
    .global Default_Handler

    /* 导入外部符号 */
    .word _sdata
    .word _edata
    .word _sbss
    .word _ebss
    .word _estack

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* 初始化栈指针 */
    LDR r0, =_estack
    MSR msp, r0

    /* 复制数据段 */
    LDR r1, =_etext
    LDR r2, =_sdata
    LDR r3, =_edata
    
LoopCopyData:
    CMP r2, r3
    BGE LoopZeroBss
    LDR r0, [r1], #4
    STR r0, [r2], #4
    B LoopCopyData

LoopZeroBss:
    LDR r2, =_sbss
    LDR r3, =_ebss
    MOV r0, #0

FillZeroBss:
    CMP r2, r3
    BGE CallMain
    STR r0, [r2], #4
    B FillZeroBss

CallMain:
    BL main
    BX LR
    .size Reset_Handler, .-Reset_Handler

    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    B Infinite_Loop
    .size Default_Handler, .-Default_Handler

    /* HardFault 处理程序 - 用于调试 */
    .section .text.HardFault_Handler,"ax",%progbits
    .weak HardFault_Handler
    .type HardFault_Handler, %function
HardFault_Handler:
    /* 进入死循环 - 如果进入这里，说明触发了 HardFault */
    /* 可以通过调试器查看寄存器来诊断问题 */
    B HardFault_Handler
    .size HardFault_Handler, .-HardFault_Handler

    .section .isr_vector,"a",%progbits
    .type g_pfnVectors, %object
    
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler /* NMI */
    .word HardFault_Handler /* HardFault */
    .word Default_Handler /* MemManage */
    .word Default_Handler /* BusFault */
    .word Default_Handler /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler     /* SVC - OS Task Start */
    .word Default_Handler /* DebugMon */
    .word 0
    .word PendSV_Handler  /* PendSV - OS Context Switch */
    .word SysTick_Handler /* SysTick - OS Tick */
    /* External Interrupts */
    .word Default_Handler /* GPIO Port A */
    .word Default_Handler /* GPIO Port B */
    .word Default_Handler /* GPIO Port C */
    .word Default_Handler /* GPIO Port D */
    .word Default_Handler /* GPIO Port E */
    .word UART0_Handler   /* UART0 - Interrupt #5 */

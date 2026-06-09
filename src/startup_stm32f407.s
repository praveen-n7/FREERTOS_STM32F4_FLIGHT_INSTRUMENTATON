.syntax unified
  .cpu cortex-m4
  .thumb

/* ── Stack top (defined in linker script) ── */
  .global _estack

/* ── Reset handler ── */
  .global Reset_Handler

/* ── External symbols from linker script ── */
  .extern _sidata
  .extern _sdata
  .extern _edata
  .extern _sbss
  .extern _ebss
  .extern SystemInit
  .extern main

/* ════════════════════════════════════════════
   Vector Table — must be at 0x08000000
   ════════════════════════════════════════════ */
  .section .isr_vector, "a", %progbits
  .type g_pfnVectors, %object

g_pfnVectors:
  .word _estack                        /* 0  Stack top */
  .word Reset_Handler                  /* 1  Reset */
  .word Default_Handler                /* 2  NMI */
  .word Default_Handler                /* 3  HardFault */
  .word Default_Handler                /* 4  MemManage */
  .word Default_Handler                /* 5  BusFault */
  .word Default_Handler                /* 6  UsageFault */
  .word 0                              /* 7  Reserved */
  .word 0                              /* 8  Reserved */
  .word 0                              /* 9  Reserved */
  .word 0                              /* 10 Reserved */
  .word Default_Handler                /* 11 SVCall */
  .word Default_Handler                /* 12 Debug Monitor */
  .word 0                              /* 13 Reserved */
  .word Default_Handler                /* 14 PendSV */
  .word Default_Handler                /* 15 SysTick */

  /* External interrupts (IRQ0 to IRQ81) */
  .word Default_Handler                /* IRQ0  WWDG */
  .word Default_Handler                /* IRQ1  PVD */
  .word Default_Handler                /* IRQ2  TAMP_STAMP */
  .word Default_Handler                /* IRQ3  RTC_WKUP */
  .word Default_Handler                /* IRQ4  FLASH */
  .word Default_Handler                /* IRQ5  RCC */
  .word Default_Handler                /* IRQ6  EXTI0 */
  .word Default_Handler                /* IRQ7  EXTI1 */
  .word Default_Handler                /* IRQ8  EXTI2 */
  .word Default_Handler                /* IRQ9  EXTI3 */
  .word Default_Handler                /* IRQ10 EXTI4 */
  .word Default_Handler                /* IRQ11 DMA1_Stream0 */
  .word Default_Handler                /* IRQ12 DMA1_Stream1 */
  .word Default_Handler                /* IRQ13 DMA1_Stream2 */
  .word Default_Handler                /* IRQ14 DMA1_Stream3 */
  .word Default_Handler                /* IRQ15 DMA1_Stream4 */
  .word Default_Handler                /* IRQ16 DMA1_Stream5 */
  .word Default_Handler                /* IRQ17 DMA1_Stream6 */
  .word Default_Handler                /* IRQ18 ADC */
  .word Default_Handler                /* IRQ19 CAN1_TX */
  .word Default_Handler                /* IRQ20 CAN1_RX0 */
  .word Default_Handler                /* IRQ21 CAN1_RX1 */
  .word Default_Handler                /* IRQ22 CAN1_SCE */
  .word Default_Handler                /* IRQ23 EXTI9_5 */
  .word Default_Handler                /* IRQ24 TIM1_BRK_TIM9 */
  .word Default_Handler                /* IRQ25 TIM1_UP_TIM10 */
  .word Default_Handler                /* IRQ26 TIM1_TRG_COM_TIM11 */
  .word Default_Handler                /* IRQ27 TIM1_CC */
  .word Default_Handler                /* IRQ28 TIM2 */
  .word Default_Handler                /* IRQ29 TIM3 */
  .word Default_Handler                /* IRQ30 TIM4 */
  .word Default_Handler                /* IRQ31 I2C1_EV */
  .word Default_Handler                /* IRQ32 I2C1_ER */
  .word Default_Handler                /* IRQ33 I2C2_EV */
  .word Default_Handler                /* IRQ34 I2C2_ER */
  .word Default_Handler                /* IRQ35 SPI1 */
  .word Default_Handler                /* IRQ36 SPI2 */
  .word Default_Handler                /* IRQ37 USART1 */
  .word USART2_IRQHandler              /* IRQ38 USART2 */
  .word Default_Handler                /* IRQ39 USART3 */
  .word Default_Handler                /* IRQ40 EXTI15_10 */
  .word Default_Handler                /* IRQ41 RTC_Alarm */
  .word Default_Handler                /* IRQ42 OTG_FS_WKUP */
  .word Default_Handler                /* IRQ43 TIM8_BRK_TIM12 */
  .word Default_Handler                /* IRQ44 TIM8_UP_TIM13 */
  .word Default_Handler                /* IRQ45 TIM8_TRG_COM_TIM14 */
  .word Default_Handler                /* IRQ46 TIM8_CC */
  .word Default_Handler                /* IRQ47 DMA1_Stream7 */
  .word Default_Handler                /* IRQ48 FMC */
  .word Default_Handler                /* IRQ49 SDIO */
  .word Default_Handler                /* IRQ50 TIM5 */
  .word Default_Handler                /* IRQ51 SPI3 */
  .word Default_Handler                /* IRQ52 UART4 */
  .word Default_Handler                /* IRQ53 UART5 */
  .word Default_Handler                /* IRQ54 TIM6_DAC */
  .word Default_Handler                /* IRQ55 TIM7 */
  .word Default_Handler                /* IRQ56 DMA2_Stream0 */
  .word Default_Handler                /* IRQ57 DMA2_Stream1 */
  .word Default_Handler                /* IRQ58 DMA2_Stream2 */
  .word Default_Handler                /* IRQ59 DMA2_Stream3 */
  .word Default_Handler                /* IRQ60 DMA2_Stream4 */
  .word Default_Handler                /* IRQ61 ETH */
  .word Default_Handler                /* IRQ62 ETH_WKUP */
  .word Default_Handler                /* IRQ63 CAN2_TX */
  .word Default_Handler                /* IRQ64 CAN2_RX0 */
  .word Default_Handler                /* IRQ65 CAN2_RX1 */
  .word Default_Handler                /* IRQ66 CAN2_SCE */
  .word Default_Handler                /* IRQ67 OTG_FS */
  .word Default_Handler                /* IRQ68 DMA2_Stream5 */
  .word Default_Handler                /* IRQ69 DMA2_Stream6 */
  .word Default_Handler                /* IRQ70 DMA2_Stream7 */
  .word Default_Handler                /* IRQ71 USART6 */
  .word Default_Handler                /* IRQ72 I2C3_EV */
  .word Default_Handler                /* IRQ73 I2C3_ER */
  .word Default_Handler                /* IRQ74 OTG_HS_EP1_OUT */
  .word Default_Handler                /* IRQ75 OTG_HS_EP1_IN */
  .word Default_Handler                /* IRQ76 OTG_HS_WKUP */
  .word Default_Handler                /* IRQ77 OTG_HS */
  .word Default_Handler                /* IRQ78 DCMI */
  .word Default_Handler                /* IRQ79 CRYP */
  .word Default_Handler                /* IRQ80 HASH_RNG */
  .word Default_Handler                /* IRQ81 FPU */

/* ════════════════════════════════════════════
   Reset Handler — copies .data, zeros .bss, calls main
   ════════════════════════════════════════════ */
  .section .text.Reset_Handler, "ax", %progbits
  .weak Reset_Handler
  .type Reset_Handler, %function

Reset_Handler:
  /* Copy .data from Flash to RAM */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4

LoopCopyDataInit:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   CopyDataInit

  /* Zero .bss */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     LoopFillZerobss

FillZerobss:
  str   r3, [r2]
  adds  r2, r2, #4

LoopFillZerobss:
  cmp   r2, r4
  bcc   FillZerobss

  /* Call SystemInit then main */
  bl    SystemInit
  bl    main
  bx    lr

/* ════════════════════════════════════════════
   Default Handler — infinite loop on any unhandled IRQ
   ════════════════════════════════════════════ */
  .section .text.Default_Handler, "ax", %progbits
  .weak Default_Handler
  .type Default_Handler, %function

Default_Handler:
  b     Default_Handler

  /* Make USART2_IRQHandler weak so uart.c can override it */
  .weak USART2_IRQHandler
  .thumb_set USART2_IRQHandler, Default_Handler
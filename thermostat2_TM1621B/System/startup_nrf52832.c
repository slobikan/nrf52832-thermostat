#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Cortex-M4 system handlers */
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)__attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));

/* nRF52832 peripheral IRQs (IRQ0..IRQ31) */
void POWER_CLOCK_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void RADIO_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void UARTE0_UART0_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void SPIM0_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void SPIM1_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void GPIOTE_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void SAADC_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIMER0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void TIMER1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void TIMER2_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void RTC0_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TEMP_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void RNG_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void ECB_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void CCM_AAR_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void WDT_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void RTC1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void QDEC_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void COMP_LPCOMP_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void SWI0_EGU0_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void SWI1_EGU1_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void SWI2_EGU2_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void SWI3_EGU3_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void SWI4_EGU4_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void SWI5_EGU5_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void TIMER3_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void TIMER4_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void PWM0_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void PDM_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void MWU_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void PWM1_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void PWM2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void SPIM2_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void RTC2_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void I2S_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void FPU_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".vectors")))
void (*const g_vectors[])(void) = {
    /* Stack pointer */
    (void (*)(void))(&_estack),
    /* Cortex-M4 exceptions */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    /* nRF52832 IRQs: position 0..38 */
    POWER_CLOCK_IRQHandler,   /* IRQ0  */
    RADIO_IRQHandler,         /* IRQ1  */
    UARTE0_UART0_IRQHandler,  /* IRQ2  */
    SPIM0_IRQHandler,         /* IRQ3  */
    SPIM1_IRQHandler,         /* IRQ4  */
    0,                        /* IRQ5  */
    GPIOTE_IRQHandler,        /* IRQ6  */
    SAADC_IRQHandler,         /* IRQ7  */
    TIMER0_IRQHandler,        /* IRQ8  */
    TIMER1_IRQHandler,        /* IRQ9  */
    TIMER2_IRQHandler,        /* IRQ10 */
    RTC0_IRQHandler,          /* IRQ11 */
    TEMP_IRQHandler,          /* IRQ12 */
    RNG_IRQHandler,           /* IRQ13 */
    ECB_IRQHandler,           /* IRQ14 */
    CCM_AAR_IRQHandler,       /* IRQ15 */
    WDT_IRQHandler,           /* IRQ16 */
    RTC1_IRQHandler,          /* IRQ17 */
    QDEC_IRQHandler,          /* IRQ18 */
    COMP_LPCOMP_IRQHandler,   /* IRQ19 */
    SWI0_EGU0_IRQHandler,     /* IRQ20 */
    SWI1_EGU1_IRQHandler,     /* IRQ21 */
    SWI2_EGU2_IRQHandler,     /* IRQ22 */
    SWI3_EGU3_IRQHandler,     /* IRQ23 */
    SWI4_EGU4_IRQHandler,     /* IRQ24 */
    SWI5_EGU5_IRQHandler,     /* IRQ25 */
    TIMER3_IRQHandler,        /* IRQ26 */
    TIMER4_IRQHandler,        /* IRQ27 */
    PWM0_IRQHandler,          /* IRQ28 */
    PDM_IRQHandler,           /* IRQ29 */
    0,                        /* IRQ30 */
    0,                        /* IRQ31 */
    MWU_IRQHandler,           /* IRQ32 */
    PWM1_IRQHandler,          /* IRQ33 */
    PWM2_IRQHandler,          /* IRQ34 */
    SPIM2_IRQHandler,         /* IRQ35 */
    RTC2_IRQHandler,          /* IRQ36 */
    I2S_IRQHandler,           /* IRQ37 */
    FPU_IRQHandler,           /* IRQ38 */
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0u;
    }

    (void)main();
    while (1) {}
}

void Default_Handler(void)
{
    while (1) {}
}

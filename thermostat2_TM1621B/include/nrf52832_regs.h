#ifndef NRF52832_REGS_H
#define NRF52832_REGS_H

#include <stdint.h>

#define __IO volatile
#define __I  volatile const

#define NRF_CLOCK_BASE  (0x40000000u)
#define NRF_TIMER0_BASE (0x40008000u)
#define NRF_RADIO_BASE  (0x40001000u)
#define NRF_GPIO_BASE   (0x50000000u)
#define NRF_SAADC_BASE  (0x40007000u)
#define NRF_NVMC_BASE   (0x4001E000u)
#define NRF_FICR_BASE   (0x10000000u)

typedef struct {
    __IO uint32_t TASKS_HFCLKSTART;   /* 0x000 */
    __IO uint32_t TASKS_HFCLKSTOP;    /* 0x004 */
    __IO uint32_t TASKS_LFCLKSTART;   /* 0x008 */
    __IO uint32_t TASKS_LFCLKSTOP;    /* 0x00C */
    uint32_t _r0[60];                  /* 0x010..0x0FC */
    __IO uint32_t EVENTS_HFCLKSTARTED; /* 0x100 */
    uint32_t _r1[194];                 /* 0x104..0x408, gap=(0x40C-0x104)/4=194 */
    __IO uint32_t HFCLKSTAT;           /* 0x40C — бит 16: STATE=1 если HFCLK running */
} NRF_CLOCK_Type;

typedef struct {
    __IO uint32_t TASKS_START;
    __IO uint32_t TASKS_STOP;
    __IO uint32_t TASKS_COUNT;
    __IO uint32_t TASKS_CLEAR;
    uint32_t _r0[60];
    __IO uint32_t TASKS_CAPTURE[4];
    uint32_t _r1[44];
    __IO uint32_t EVENTS_COMPARE[4];
    uint32_t _r2[44];
    __IO uint32_t SHORTS;
    uint32_t _r3[64];
    __IO uint32_t INTENSET;
    __IO uint32_t INTENCLR;
    uint32_t _r4[126];
    __IO uint32_t MODE;
    __IO uint32_t BITMODE;
    uint32_t _r5;
    __IO uint32_t PRESCALER;
    uint32_t _r6[11];
    __IO uint32_t CC[4];
} NRF_TIMER_Type;

typedef struct {
    __IO uint32_t TASKS_TXEN;
    __IO uint32_t TASKS_RXEN;
    __IO uint32_t TASKS_START;
    __IO uint32_t TASKS_STOP;
    __IO uint32_t TASKS_DISABLE;
    uint32_t _r0[59];
    __IO uint32_t EVENTS_READY;
    __IO uint32_t EVENTS_ADDRESS;
    __IO uint32_t EVENTS_PAYLOAD;
    __IO uint32_t EVENTS_END;
    __IO uint32_t EVENTS_DISABLED;
    uint32_t _r1[7];
    __IO uint32_t EVENTS_CRCOK;
    __IO uint32_t EVENTS_CRCERROR;
    uint32_t _r2[50];
    __IO uint32_t SHORTS;
    uint32_t _r3[64];
    __IO uint32_t INTENSET;
    __IO uint32_t INTENCLR;
    uint32_t _r4[61];
    __IO uint32_t CRCSTATUS;
    __IO uint32_t RXMATCH;
    __IO uint32_t RXCRC;
    __IO uint32_t DAI;
    uint32_t _r5[61];
    __IO uint32_t PACKETPTR;
    __IO uint32_t FREQUENCY;
    __IO uint32_t TXPOWER;
    __IO uint32_t MODE;
    __IO uint32_t PCNF0;
    __IO uint32_t PCNF1;
    __IO uint32_t BASE0;
    __IO uint32_t BASE1;
    __IO uint32_t PREFIX0;
    __IO uint32_t PREFIX1;
    __IO uint32_t TXADDRESS;
    __IO uint32_t RXADDRESSES;
    __IO uint32_t CRCCNF;
    __IO uint32_t CRCPOLY;
    __IO uint32_t CRCINIT;
    uint32_t _r6;
    __IO uint32_t TIFS;
    __IO uint32_t RSSISAMPLE;
    uint32_t _r7[1];
    __IO uint32_t STATE;
    __IO uint32_t DATAWHITEIV;
    uint32_t _r8[2];
    __IO uint32_t BCC;
    uint32_t _r9[678];
    __IO uint32_t POWER;
} NRF_RADIO_Type;

typedef struct {
    uint32_t _r0[321];
    __IO uint32_t OUT;
    __IO uint32_t OUTSET;
    __IO uint32_t OUTCLR;
    __IO uint32_t IN;
    __IO uint32_t DIR;
    __IO uint32_t DIRSET;
    __IO uint32_t DIRCLR;
    uint32_t _r1[120];
    __IO uint32_t PIN_CNF[32];
} NRF_GPIO_Type;

typedef struct {
    __IO uint32_t TASKS_START;
    __IO uint32_t TASKS_SAMPLE;
    __IO uint32_t TASKS_STOP;
    __IO uint32_t TASKS_CALIBRATEOFFSET;
    uint32_t _r0[60];
    __IO uint32_t EVENTS_STARTED;
    __IO uint32_t EVENTS_END;
    __IO uint32_t EVENTS_DONE;
    __IO uint32_t EVENTS_RESULTDONE;
    __IO uint32_t EVENTS_CALIBRATEDONE;
    __IO uint32_t EVENTS_STOPPED;
    uint32_t _r1[250];
    __IO uint32_t ENABLE;
    uint32_t _r2[3];
    __IO uint32_t CH0_PSELP;
    __IO uint32_t CH0_PSELN;
    __IO uint32_t CH0_CONFIG;
    uint32_t _r3[258];
    __IO uint32_t RESULT_PTR;
    __IO uint32_t RESULT_MAXCNT;
    __IO uint32_t RESULT_AMOUNT;
} NRF_SAADC_Type;

typedef struct {
    uint32_t _nvmc_r0[256];      /* 0x000..0x3FC */
    __IO uint32_t READY;         /* 0x400 */
    uint32_t _nvmc_r1[64];       /* 0x404..0x500 */
    __IO uint32_t CONFIG;        /* 0x504 */
    __IO uint32_t ERASEPAGE;     /* 0x508 */
    __IO uint32_t ERASEALL;      /* 0x50C */
    uint32_t _nvmc_r2;           /* 0x510 */
    __IO uint32_t ERASEUICR;     /* 0x514 */
} NRF_NVMC_Type;

typedef struct {
    uint32_t _r0[24];
    __I uint32_t DEVICEID[2];
} NRF_FICR_Type;

#define NRF_CLOCK  ((NRF_CLOCK_Type *)NRF_CLOCK_BASE)
#define NRF_TIMER0 ((NRF_TIMER_Type *)NRF_TIMER0_BASE)
#define NRF_RADIO  ((NRF_RADIO_Type *)NRF_RADIO_BASE)
#define NRF_P0     ((NRF_GPIO_Type *)NRF_GPIO_BASE)
#define NRF_SAADC  ((NRF_SAADC_Type *)NRF_SAADC_BASE)
#define NRF_NVMC   ((NRF_NVMC_Type *)NRF_NVMC_BASE)
#define NRF_FICR   ((NRF_FICR_Type *)NRF_FICR_BASE)

/* ================================================================
 * Дополнения для low-power режима
 * ================================================================ */

/* --- RTC0 (32 kHz, для millis() без HFCLK) --- */
#define NRF_RTC0_BASE   (0x4000B000u)

typedef struct {
    __IO uint32_t TASKS_START;        /* 0x000 */
    __IO uint32_t TASKS_STOP;         /* 0x004 */
    __IO uint32_t TASKS_CLEAR;        /* 0x008 */
    __IO uint32_t TASKS_TRIGOVRFLW;   /* 0x00C */
    uint32_t _r0[60];                  /* 0x010..0x0FC */
    __IO uint32_t EVENTS_TICK;         /* 0x100 */
    __IO uint32_t EVENTS_OVRFLW;       /* 0x104 */
    uint32_t _r1[127];                 /* 0x108..0x300, gap=(0x304-0x108)/4=127 */
    __IO uint32_t INTENSET;            /* 0x304 */
    __IO uint32_t INTENCLR;            /* 0x308 */
    uint32_t _r2[126];                 /* 0x30C..0x500, gap=(0x504-0x30C)/4=126 */
    __IO uint32_t COUNTER;             /* 0x504 */
    __IO uint32_t PRESCALER;           /* 0x508 */
} NRF_RTC_Type;

#define NRF_RTC0    ((NRF_RTC_Type *)NRF_RTC0_BASE)

/* --- CLOCK: запуск LFCLK (для RTC) --- */
/* Уже есть NRF_CLOCK_Type, добавляем нужные поля через отдельный доступ */
#define NRF_CLOCK_TASKS_LFCLKSTART   (*((volatile uint32_t *)(NRF_CLOCK_BASE + 0x008u)))
#define NRF_CLOCK_EVENTS_LFCLKSTARTED (*((volatile uint32_t *)(NRF_CLOCK_BASE + 0x104u)))
#define NRF_CLOCK_LFCLKSRC            (*((volatile uint32_t *)(NRF_CLOCK_BASE + 0x518u)))

/* --- NVIC (Cortex-M4 NVIC, базовый адрес 0xE000E000) --- */
#define NVIC_ISER_BASE  ((volatile uint32_t *)0xE000E100u)  /* Set-enable  */
#define NVIC_ICER_BASE  ((volatile uint32_t *)0xE000E180u)  /* Clear-enable */
#define NVIC_ICPR_BASE  ((volatile uint32_t *)0xE000E280u)  /* Clear-pending */
#define NVIC_IPR_BASE   ((volatile uint8_t  *)0xE000E400u)  /* Priority     */

/* SCB: System Control Register — бит SEVONPEND нужен для WFE на events */
#define SCB_SCR         (*((volatile uint32_t *)0xE000ED10u))
#define SCB_SCR_SEVONPEND  (1u << 4)

/* Включить/выключить IRQ по номеру */
static inline void nvic_enable_irq(uint32_t irq)
{
    NVIC_ISER_BASE[irq >> 5u] = (1u << (irq & 0x1Fu));
}
static inline void nvic_disable_irq(uint32_t irq)
{
    NVIC_ICER_BASE[irq >> 5u] = (1u << (irq & 0x1Fu));
}
static inline void nvic_clear_pending(uint32_t irq)
{
    NVIC_ICPR_BASE[irq >> 5u] = (1u << (irq & 0x1Fu));
}
static inline void nvic_set_priority(uint32_t irq, uint8_t prio)
{
    NVIC_IPR_BASE[irq] = (uint8_t)(prio << 4u); /* верхние 4 бита */
}

/* nRF52832 IRQ номера */
#define IRQ_RADIO   1u
#define IRQ_RTC0    11u

/* --- WFE / SEV / DSB / ISB inline --- */
static inline void cpu_wfe(void)   { __asm volatile ("wfe" ::: "memory"); }
static inline void cpu_sev(void)   { __asm volatile ("sev" ::: "memory"); }
static inline void cpu_dsb(void)   { __asm volatile ("dsb" ::: "memory"); }
static inline void cpu_isb(void)   { __asm volatile ("isb" ::: "memory"); }

/* Глобальный enable/disable прерываний */
static inline void cpu_irq_enable(void)  { __asm volatile ("cpsie i" ::: "memory"); }
static inline void cpu_irq_disable(void) { __asm volatile ("cpsid i" ::: "memory"); }

#endif /* NRF52832_REGS_H */

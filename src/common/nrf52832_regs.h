#ifndef NRF52832_REGS_H
#define NRF52832_REGS_H

#include <stdint.h>

#define __IO volatile
#define __I  volatile const

#define BIT(n) (1UL << (n))

typedef struct {
    __IO uint32_t TASKS_OUT[2];
    uint32_t _rsv0[6];
    __IO uint32_t TASKS_SET[2];
    uint32_t _rsv1[6];
    __IO uint32_t TASKS_CLR[2];
    uint32_t _rsv2[118];
    __IO uint32_t OUT;
    __IO uint32_t OUTSET;
    __IO uint32_t OUTCLR;
    __I  uint32_t IN;
    __IO uint32_t DIR;
    __IO uint32_t DIRSET;
    __IO uint32_t DIRCLR;
    uint32_t _rsv3[120];
    __IO uint32_t PIN_CNF[32];
} NRF_GPIO_Type;

typedef struct {
    __IO uint32_t TASKS_STARTRX;
    __IO uint32_t TASKS_STOPRX;
    __IO uint32_t TASKS_STARTTX;
    __IO uint32_t TASKS_STOPTX;
    uint32_t _rsv0;
    __IO uint32_t TASKS_DISABLE;
    uint32_t _rsv1[56];
    __IO uint32_t EVENTS_READY;
    uint32_t _rsv2[2];
    __IO uint32_t EVENTS_END;
    uint32_t _rsv3[4];
    __IO uint32_t EVENTS_DISABLED;
    uint32_t _rsv4[47];
    __IO uint32_t SHORTS;
    uint32_t _rsv5[64];
    __IO uint32_t INTENSET;
    __IO uint32_t INTENCLR;
    uint32_t _rsv6[61];
    __IO uint32_t CRCSTATUS;
    uint32_t _rsv7;
    __IO uint32_t RXMATCH;
    __IO uint32_t RXCRC;
    __IO uint32_t DAI;
    uint32_t _rsv8[60];
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
    uint32_t _rsv9;
    __IO uint32_t TIFS;
    __IO uint32_t RSSISAMPLE;
    uint32_t _rsv10;
    __IO uint32_t STATE;
    __IO uint32_t DATAWHITEIV;
    uint32_t _rsv11[2];
    __IO uint32_t BCC;
} NRF_RADIO_Type;

typedef struct {
    __IO uint32_t TASKS_HFCLKSTART;
    __IO uint32_t TASKS_HFCLKSTOP;
    uint32_t _rsv0[62];
    __IO uint32_t EVENTS_HFCLKSTARTED;
} NRF_CLOCK_Type;

typedef struct {
    __IO uint32_t READY;
    uint32_t _rsv0[64];
    __IO uint32_t CONFIG;
    __IO uint32_t ERASEPAGE;
    __IO uint32_t ERASEALL;
    uint32_t _rsv1[5];
    __IO uint32_t ERASEUICR;
} NRF_NVMC_Type;

#define NRF_P0      ((NRF_GPIO_Type *)0x50000000UL)
#define NRF_RADIO   ((NRF_RADIO_Type*)0x40001000UL)
#define NRF_CLOCK   ((NRF_CLOCK_Type*)0x40000000UL)
#define NRF_NVMC    ((NRF_NVMC_Type *)0x4001E000UL)

#endif

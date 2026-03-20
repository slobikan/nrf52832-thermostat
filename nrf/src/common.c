#include "common.h"
#include "nrf52832_regs.h"

/* ------------------------------------------------------------------ */
/* SAADC регистры (direct offset access)                               */
/* ------------------------------------------------------------------ */
#define SAADC_REG32(off) (*((volatile uint32_t *)(NRF_SAADC_BASE + (off))))
#define SAADC_TASKS_START_OFF      0x000u
#define SAADC_TASKS_SAMPLE_OFF     0x004u
#define SAADC_TASKS_STOP_OFF       0x008u
#define SAADC_TASKS_CALIBRATE_OFF  0x00Cu
#define SAADC_EVENTS_STARTED_OFF   0x100u
#define SAADC_EVENTS_END_OFF       0x104u
#define SAADC_EVENTS_STOPPED_OFF   0x114u
#define SAADC_EVENTS_CALDONE_OFF   0x110u
#define SAADC_ENABLE_OFF           0x500u
#define SAADC_CH0_PSELP_OFF        0x510u
#define SAADC_CH0_PSELN_OFF        0x514u
#define SAADC_CH0_CONFIG_OFF       0x518u
#define SAADC_RESOLUTION_OFF       0x5F0u
#define SAADC_RESULT_PTR_OFF       0x62Cu
#define SAADC_RESULT_MAXCNT_OFF    0x630u

static int16_t g_saadc_sample;

static bool saadc_wait_event(uint32_t event_off, uint32_t guard_max)
{
    uint32_t guard = 0u;
    while ((SAADC_REG32(event_off) == 0u) && (guard < guard_max)) {
        ++guard;
    }
    return (SAADC_REG32(event_off) != 0u);
}

/* ================================================================
 * CLOCK
 * ================================================================ */

void clock_hf_start(void)
{
    /* HFCLKSTAT bit 0 = source (1 => crystal), bit 16 = running. */
    if ((NRF_CLOCK->HFCLKSTAT & ((1u << 16) | 1u)) == ((1u << 16) | 1u)) {
        return;
    }

    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0u;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1u;

    /* Wait until the external crystal becomes the active HF source. */
    uint32_t guard = 0u;
    while (((NRF_CLOCK->HFCLKSTAT & ((1u << 16) | 1u)) != ((1u << 16) | 1u))
            && (guard < 200000u)) {
        ++guard;
    }
}

/* FIX #1: выключаем HFCLK когда радио не нужно → -400..500 µA */
void clock_hf_stop(void)
{
    NRF_CLOCK->TASKS_HFCLKSTOP = 1u;
}

/* Запускаем LFCLK (RC 32 kHz) — нужен для RTC */
static void clock_lf_start(void)
{
    /* LFCLKSRC: 0=RC внутренний, 1=Xtal, 2=Synth */
    NRF_CLOCK_LFCLKSRC = 0u;  /* RC oscillator — не требует внешнего кварца */
    NRF_CLOCK_EVENTS_LFCLKSTARTED = 0u;
    NRF_CLOCK_TASKS_LFCLKSTART    = 1u;
    while (NRF_CLOCK_EVENTS_LFCLKSTARTED == 0u) {}
}

/* ================================================================
 * RTC0 вместо TIMER0 — потребление ~1-2 µA vs ~500 µA у TIMER
 *
 * Prescaler = 32-1 → 32768/32 = 1024 Гц (тик ~0.977 мс)
 * Для millis() делаем *1000/1024 ≈ >>10 (быстро, без деления)
 * Точность ~2.4% — достаточно для термостата.
 *
 * Счётчик RTC 24-бит → переполнение через 16 777 215 / 1024 ≈ 16383 с ≈ 4.5 часа
 * При использовании (millis()-start) < timeout это безопасно.
 * ================================================================ */
void timer0_init_1mhz(void)
{
    /* Имя функции оставлено для совместимости с вызовами в main() */
    clock_lf_start();

    NRF_RTC0->TASKS_STOP  = 1u;
    NRF_RTC0->TASKS_CLEAR = 1u;
    NRF_RTC0->PRESCALER   = 32u - 1u;  /* 32768/32 = 1024 Гц */

    /* P0 FIX #1: включаем TICK IRQ — гарантированный периодический источник
     * пробуждения для WFE в delay_ms(). Без него после radio_power_down()
     * cpu_wfe() может спать вечно если больше нет активных IRQ источников.
     * TICK @ 1024 Гц = прерывание каждые ~1 мс — пробуждает WFE минимум раз в мс. */
    NRF_RTC0->INTENSET = (1u << 0);  /* бит 0 = TICK interrupt enable */

    /* Включаем RTC0 IRQ в NVIC (IRQ11) */
    nvic_clear_pending(IRQ_RTC0);
    nvic_set_priority(IRQ_RTC0, 3u);  /* низкий приоритет, ниже RADIO */
    nvic_enable_irq(IRQ_RTC0);

    NRF_RTC0->TASKS_START = 1u;
}

/* RTC0 IRQ обработчик — только сбрасывает TICK событие.
 * Сам факт входа в IRQ будит CPU из WFE. */
void RTC0_IRQHandler(void)
{
    NRF_RTC0->EVENTS_TICK = 0u;
}

uint32_t millis(void)
{
    /* 1024 тика/с → millis = counter * 1000 / 1024 ≈ counter * 125 / 128 */
    uint32_t ticks = NRF_RTC0->COUNTER;
    return (ticks * 125u) >> 7u;  /* >>7 == /128, итого *125/128 ≈ *1000/1024 */
}

void delay_ms(uint32_t ms)
{
    /* Overflow-safe delay based on RTC millis(). */
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        cpu_wfe();
    }
}

/* ================================================================
 * GPIO
 * ================================================================ */

void gpio_input_pullup(uint32_t pin)
{
    NRF_P0->DIRCLR      = (1u << pin);
    NRF_P0->PIN_CNF[pin] = (0u << 0) | (3u << 2) | (0u << 8) | (0u << 16);
}

void gpio_output(uint32_t pin)
{
    NRF_P0->DIRSET      = (1u << pin);
    NRF_P0->PIN_CNF[pin] = (1u << 0) | (0u << 1) | (0u << 2) | (0u << 8) | (0u << 16);
}

void gpio_set(uint32_t pin)   { NRF_P0->OUTSET = (1u << pin); }
void gpio_clear(uint32_t pin) { NRF_P0->OUTCLR = (1u << pin); }
bool gpio_read(uint32_t pin)  { return ((NRF_P0->IN >> pin) & 1u) != 0u; }

/* ================================================================
 * SAADC
 * ================================================================ */

void saadc_init_ain2_p04(void)
{
    SAADC_REG32(SAADC_ENABLE_OFF) = 1u;

    /* AIN2 (P0.04), single-ended */
    SAADC_REG32(SAADC_CH0_PSELP_OFF) = 3u;
    SAADC_REG32(SAADC_CH0_PSELN_OFF) = 0u;

    /* GAIN=1/4, REF=Internal, TACQ=10us, MODE=SE */
    SAADC_REG32(SAADC_CH0_CONFIG_OFF) =
        (0u << 0)  |
        (0u << 4)  |
        (2u << 8)  |
        (0u << 12) |
        (2u << 16) |
        (0u << 20) |
        (0u << 24);

    SAADC_REG32(SAADC_RESOLUTION_OFF)    = 2u;  /* 12-bit */
    SAADC_REG32(SAADC_RESULT_PTR_OFF)    = (uint32_t)&g_saadc_sample;
    SAADC_REG32(SAADC_RESULT_MAXCNT_OFF) = 1u;

    /* FIX #7: калибровка перед первым измерением */
    SAADC_REG32(SAADC_EVENTS_CALDONE_OFF)  = 0u;
    SAADC_REG32(SAADC_EVENTS_STARTED_OFF)  = 0u;
    SAADC_REG32(SAADC_EVENTS_STOPPED_OFF)  = 0u;

    SAADC_REG32(SAADC_TASKS_START_OFF) = 1u;
    if (!saadc_wait_event(SAADC_EVENTS_STARTED_OFF, 200000u)) {
        return;
    }

    SAADC_REG32(SAADC_TASKS_CALIBRATE_OFF) = 1u;
    if (!saadc_wait_event(SAADC_EVENTS_CALDONE_OFF, 400000u)) {
        return;
    }

    SAADC_REG32(SAADC_TASKS_STOP_OFF) = 1u;
    (void)saadc_wait_event(SAADC_EVENTS_STOPPED_OFF, 200000u);

    SAADC_REG32(SAADC_EVENTS_STARTED_OFF) = 0u;
    SAADC_REG32(SAADC_EVENTS_STOPPED_OFF) = 0u;
    SAADC_REG32(SAADC_EVENTS_CALDONE_OFF) = 0u;
}

int16_t saadc_sample_raw(void)
{
    SAADC_REG32(SAADC_EVENTS_STARTED_OFF) = 0u;
    SAADC_REG32(SAADC_EVENTS_END_OFF)     = 0u;
    SAADC_REG32(SAADC_EVENTS_STOPPED_OFF) = 0u;

    SAADC_REG32(SAADC_RESULT_PTR_OFF)    = (uint32_t)&g_saadc_sample;
    SAADC_REG32(SAADC_RESULT_MAXCNT_OFF) = 1u;

    SAADC_REG32(SAADC_TASKS_START_OFF) = 1u;
    if (!saadc_wait_event(SAADC_EVENTS_STARTED_OFF, 200000u)) {
        return g_saadc_sample;
    }

    SAADC_REG32(SAADC_TASKS_SAMPLE_OFF) = 1u;
    if (!saadc_wait_event(SAADC_EVENTS_END_OFF, 200000u)) {
        SAADC_REG32(SAADC_TASKS_STOP_OFF) = 1u;
        (void)saadc_wait_event(SAADC_EVENTS_STOPPED_OFF, 200000u);
        return g_saadc_sample;
    }

    SAADC_REG32(SAADC_TASKS_STOP_OFF) = 1u;
    (void)saadc_wait_event(SAADC_EVENTS_STOPPED_OFF, 200000u);

    return g_saadc_sample;
}

/* ================================================================
 * Утилиты
 * ================================================================ */

uint8_t app_checksum3(uint8_t a, uint8_t b, uint8_t c)
{
    return (uint8_t)(a ^ b ^ c ^ 0xA5u);
}

uint8_t app_checksum4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (uint8_t)(a ^ b ^ c ^ d ^ 0x5Au);
}

uint8_t device_unique_id8(void)
{
    return (uint8_t)(NRF_FICR->DEVICEID[0] & 0xFFu);
}

static uint8_t pair_rec_checksum(uint32_t magic, uint8_t peer)
{
    return (uint8_t)((magic & 0xFFu) ^ ((magic >> 8) & 0xFFu) ^
                     ((magic >> 16) & 0xFFu) ^ ((magic >> 24) & 0xFFu) ^
                     peer ^ 0x3Cu);
}

bool pair_record_load(pair_record_t *out)
{
    const pair_record_t *rec = (const pair_record_t *)PAIR_STORAGE_ADDR;
    if (rec->magic != PAIR_MAGIC) { return false; }
    if (rec->checksum != pair_rec_checksum(rec->magic, rec->peer_id)) { return false; }
    *out = *rec;
    return true;
}

bool pair_record_store(uint8_t peer_id)
{
    return pair_record_store_zone(peer_id, 0xFFu);
}

bool pair_record_store_zone(uint8_t peer_id, uint8_t zone_id)
{
    pair_record_t rec;
    rec.magic     = PAIR_MAGIC;
    rec.peer_id   = peer_id;
    rec.reserved0 = zone_id;
    rec.reserved1 = 0xFFu;
    rec.checksum  = pair_rec_checksum(rec.magic, rec.peer_id);

    NRF_NVMC->CONFIG    = 2u;
    while (NRF_NVMC->READY == 0u) {}
    NRF_NVMC->ERASEPAGE = PAIR_STORAGE_ADDR;
    while (NRF_NVMC->READY == 0u) {}

    NRF_NVMC->CONFIG = 1u;
    while (NRF_NVMC->READY == 0u) {}

    volatile uint32_t *dst = (volatile uint32_t *)PAIR_STORAGE_ADDR;
    dst[0] = rec.magic;
    while (NRF_NVMC->READY == 0u) {}
    dst[1] = ((uint32_t)rec.checksum  << 24) |
             ((uint32_t)rec.reserved1 << 16) |
             ((uint32_t)rec.reserved0 <<  8) |
             rec.peer_id;
    while (NRF_NVMC->READY == 0u) {}

    NRF_NVMC->CONFIG = 0u;
    while (NRF_NVMC->READY == 0u) {}

    return true;
}

bool pair_record_clear(void)
{
    NRF_NVMC->CONFIG = 2u;
    while (NRF_NVMC->READY == 0u) {}
    NRF_NVMC->ERASEPAGE = PAIR_STORAGE_ADDR;
    while (NRF_NVMC->READY == 0u) {}

    NRF_NVMC->CONFIG = 0u;
    while (NRF_NVMC->READY == 0u) {}

    return true;
}


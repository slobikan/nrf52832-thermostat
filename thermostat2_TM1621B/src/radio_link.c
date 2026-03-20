#include "common.h"
#include "nrf52832_regs.h"
#include "radio_link.h"

#define RADIO_MODE_NRF_1MBIT  0u
#define RADIO_FREQ_CH_2407    7u
#define RADIO_POWER_0DBM      0u
#define RADIO_BASE0           0xE7E7E7E7u
#define RADIO_PREFIX0         0x000000E7u

#define RADIO_SHORT_READY_START  (1u << 0)
#define RADIO_SHORT_END_DISABLE  (1u << 1)

#define RADIO_REG32(off) (*((volatile uint32_t *)(NRF_RADIO_BASE + (off))))

/* TASKS */
#define RADIO_TASKS_TXEN_OFF      0x000u
#define RADIO_TASKS_RXEN_OFF      0x004u
#define RADIO_TASKS_START_OFF     0x008u
#define RADIO_TASKS_DISABLE_OFF   0x010u
#define RADIO_TASKS_RSSISTART_OFF 0x014u

/* EVENTS */
#define RADIO_EVENTS_READY_OFF    0x100u
#define RADIO_EVENTS_ADDRESS_OFF  0x104u
#define RADIO_EVENTS_END_OFF      0x10Cu
#define RADIO_EVENTS_DISABLED_OFF 0x110u
#define RADIO_EVENTS_RSSIEND_OFF  0x11Cu
#define RADIO_EVENTS_CRCOK_OFF    0x128u
#define RADIO_EVENTS_CRCERR_OFF   0x12Cu

/* CONFIG/STATUS */
#define RADIO_SHORTS_OFF          0x200u
#define RADIO_INTENCLR_OFF        0x30Cu
#define RADIO_PACKETPTR_OFF       0x504u
#define RADIO_FREQUENCY_OFF       0x508u
#define RADIO_TXPOWER_OFF         0x50Cu
#define RADIO_MODE_OFF            0x510u
#define RADIO_PCNF0_OFF           0x514u
#define RADIO_PCNF1_OFF           0x518u
#define RADIO_BASE0_OFF           0x51Cu
#define RADIO_PREFIX0_OFF         0x524u
#define RADIO_TXADDRESS_OFF       0x52Cu
#define RADIO_RXADDRESSES_OFF     0x530u
#define RADIO_CRCCNF_OFF          0x534u
#define RADIO_CRCPOLY_OFF         0x538u
#define RADIO_CRCINIT_OFF         0x53Cu
#define RADIO_STATE_OFF           0x550u
#define RADIO_DATAWHITEIV_OFF     0x554u
#define RADIO_POWER_OFF           0xFFCu

#define RADIO_STATE_DISABLED      0u
#define RADIO_DIAG_NO_HW_CRC      1

static radio_packet_t g_rx_buf;
static radio_packet_t g_tx_buf;
static uint8_t g_last_rssi_percent;

static uint8_t radio_rssi_sample_to_percent(uint32_t sample)
{
    uint32_t abs_dbm = sample & 0x7Fu;

    if (abs_dbm <= 40u) {
        return 100u;
    }
    if (abs_dbm >= 100u) {
        return 0u;
    }
    return (uint8_t)(((100u - abs_dbm) * 100u) / 60u);
}

static bool radio_wait_event(uint32_t event_off, uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (RADIO_REG32(event_off) == 0u) {
        if ((millis() - start) >= timeout_ms) {
            return false;
        }
    }
    return true;
}

static void radio_power_up_if_needed(void)
{
    if (RADIO_REG32(RADIO_POWER_OFF) == 0u) {
        RADIO_REG32(RADIO_POWER_OFF) = 1u;
        for (volatile uint32_t d = 0u; d < 300u; ++d) {
        }
    }
}

void radio_init(void)
{
    clock_hf_start();
    radio_power_up_if_needed();

    RADIO_REG32(RADIO_MODE_OFF)       = RADIO_MODE_NRF_1MBIT;
    RADIO_REG32(RADIO_FREQUENCY_OFF)  = RADIO_FREQ_CH_2407;
    RADIO_REG32(RADIO_TXPOWER_OFF)    = RADIO_POWER_0DBM;

    RADIO_REG32(RADIO_BASE0_OFF)       = RADIO_BASE0;
    RADIO_REG32(RADIO_PREFIX0_OFF)     = RADIO_PREFIX0;
    RADIO_REG32(RADIO_TXADDRESS_OFF)   = 0u;
    RADIO_REG32(RADIO_RXADDRESSES_OFF) = 0x01u;

    /* Fixed 5-byte payload, no on-air length field.
     * With LFLEN=0 the radio uses STATLEN as the packet length. */
    RADIO_REG32(RADIO_PCNF0_OFF) = 0x00000000u;
    RADIO_REG32(RADIO_PCNF1_OFF) = (5u << 0) | (5u << 8) | (3u << 16) | (1u << 25);

#if RADIO_DIAG_NO_HW_CRC
    RADIO_REG32(RADIO_CRCCNF_OFF)  = 0u;
#else
    RADIO_REG32(RADIO_CRCCNF_OFF)  = 2u;
    RADIO_REG32(RADIO_CRCINIT_OFF) = 0xFFFFu;
    RADIO_REG32(RADIO_CRCPOLY_OFF) = 0x11021u;
#endif

    /* Whitening enabled => DATAWHITEIV must match channel index. */
    RADIO_REG32(RADIO_DATAWHITEIV_OFF) = RADIO_FREQ_CH_2407;

    RADIO_REG32(RADIO_SHORTS_OFF) = RADIO_SHORT_READY_START | RADIO_SHORT_END_DISABLE;

    RADIO_REG32(RADIO_INTENCLR_OFF) = 0xFFFFFFFFu;
    nvic_disable_irq(IRQ_RADIO);
    nvic_clear_pending(IRQ_RADIO);
}

void radio_power_down(void)
{
    if (RADIO_REG32(RADIO_STATE_OFF) != RADIO_STATE_DISABLED) {
        RADIO_REG32(RADIO_EVENTS_DISABLED_OFF) = 0u;
        RADIO_REG32(RADIO_TASKS_DISABLE_OFF)   = 1u;
        (void)radio_wait_event(RADIO_EVENTS_DISABLED_OFF, 10u);
    }
    clock_hf_stop();
}

bool radio_tx_packet(const radio_packet_t *pkt, uint32_t timeout_ms)
{
    clock_hf_start();
    radio_power_up_if_needed();

    g_tx_buf = *pkt;
    RADIO_REG32(RADIO_PACKETPTR_OFF) = (uint32_t)&g_tx_buf;

    RADIO_REG32(RADIO_EVENTS_READY_OFF)    = 0u;
    RADIO_REG32(RADIO_EVENTS_END_OFF)      = 0u;
    RADIO_REG32(RADIO_EVENTS_DISABLED_OFF) = 0u;
    cpu_dsb();

    RADIO_REG32(RADIO_TASKS_TXEN_OFF) = 1u;

    {
        bool sent = radio_wait_event(RADIO_EVENTS_END_OFF, timeout_ms);
        if (!sent) {
            RADIO_REG32(RADIO_TASKS_DISABLE_OFF) = 1u;
        }
        (void)radio_wait_event(RADIO_EVENTS_DISABLED_OFF, timeout_ms);
        return sent;
    }
}

bool radio_rx_packet(radio_packet_t *pkt, uint32_t timeout_ms, bool *crc_ok)
{
    uint32_t start;
    bool got_end = false;
    bool rssi_started = false;

    clock_hf_start();
    radio_power_up_if_needed();

    RADIO_REG32(RADIO_PACKETPTR_OFF) = (uint32_t)&g_rx_buf;

    RADIO_REG32(RADIO_EVENTS_READY_OFF)    = 0u;
    RADIO_REG32(RADIO_EVENTS_ADDRESS_OFF)  = 0u;
    RADIO_REG32(RADIO_EVENTS_RSSIEND_OFF)  = 0u;
    RADIO_REG32(RADIO_EVENTS_CRCOK_OFF)    = 0u;
    RADIO_REG32(RADIO_EVENTS_CRCERR_OFF)   = 0u;
    RADIO_REG32(RADIO_EVENTS_END_OFF)      = 0u;
    RADIO_REG32(RADIO_EVENTS_DISABLED_OFF) = 0u;
    cpu_dsb();

    RADIO_REG32(RADIO_TASKS_RXEN_OFF) = 1u;

    start = millis();
    while (!got_end) {
        if ((RADIO_REG32(RADIO_EVENTS_ADDRESS_OFF) != 0u) && !rssi_started) {
            RADIO_REG32(RADIO_EVENTS_ADDRESS_OFF) = 0u;
            RADIO_REG32(RADIO_EVENTS_RSSIEND_OFF) = 0u;
            RADIO_REG32(RADIO_TASKS_RSSISTART_OFF) = 1u;
            rssi_started = true;
        }
        if (RADIO_REG32(RADIO_EVENTS_END_OFF) != 0u) {
            got_end = true;
            break;
        }
        if ((millis() - start) >= timeout_ms) {
            break;
        }
    }

    {
        bool ok;

        if (!got_end) {
            RADIO_REG32(RADIO_TASKS_DISABLE_OFF) = 1u;
        }

#if RADIO_DIAG_NO_HW_CRC
        ok = got_end;
#else
        ok = (RADIO_REG32(RADIO_EVENTS_CRCOK_OFF) != 0u);
#endif
        *crc_ok = ok;

        (void)radio_wait_event(RADIO_EVENTS_DISABLED_OFF, timeout_ms);

        RADIO_REG32(RADIO_EVENTS_CRCOK_OFF)    = 0u;
        RADIO_REG32(RADIO_EVENTS_CRCERR_OFF)   = 0u;
        RADIO_REG32(RADIO_EVENTS_END_OFF)      = 0u;
        RADIO_REG32(RADIO_EVENTS_DISABLED_OFF) = 0u;
        RADIO_REG32(RADIO_EVENTS_ADDRESS_OFF)  = 0u;
        RADIO_REG32(RADIO_EVENTS_RSSIEND_OFF)  = 0u;

        if (!got_end || !ok) {
            return false;
        }
    }

    if (rssi_started) {
        g_last_rssi_percent = radio_rssi_sample_to_percent(NRF_RADIO->RSSISAMPLE);
    } else {
        g_last_rssi_percent = 0u;
    }

    *pkt = g_rx_buf;
    return true;
}

uint8_t radio_last_rssi_percent(void)
{
    return g_last_rssi_percent;
}

void packet_make_telemetry(radio_packet_t *pkt, uint16_t temp_x10, uint8_t setpoint, uint8_t tx_id)
{
    pkt->b0 = (uint8_t)(temp_x10 & 0xFFu);
    pkt->b1 = (uint8_t)((temp_x10 >> 8) & 0xFFu);
    pkt->b2 = setpoint;
    pkt->b3 = tx_id;
    pkt->b4 = app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3);
}

bool packet_parse_telemetry(const radio_packet_t *pkt, uint16_t *temp_x10, uint8_t *setpoint, uint8_t *tx_id)
{
    if (pkt->b4 != app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3)) {
        return false;
    }
    *temp_x10 = (uint16_t)pkt->b0 | ((uint16_t)pkt->b1 << 8);
    *setpoint = pkt->b2;
    *tx_id = pkt->b3;
    return true;
}

void packet_make_pair_req(radio_packet_t *pkt, uint8_t tx_id, uint8_t nonce)
{
    pkt->b0 = 0x50u;
    pkt->b1 = tx_id;
    pkt->b2 = nonce;
    pkt->b3 = 0u;
    pkt->b4 = app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3);
}

bool packet_parse_pair_req(const radio_packet_t *pkt, uint8_t *tx_id, uint8_t *nonce)
{
    if (pkt->b0 != 0x50u) {
        return false;
    }
    if (pkt->b4 != app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3)) {
        return false;
    }
    *tx_id = pkt->b1;
    *nonce = pkt->b2;
    return true;
}

void packet_make_pair_ack(radio_packet_t *pkt, uint8_t rx_id, uint8_t nonce)
{
    pkt->b0 = 0xA5u;
    pkt->b1 = rx_id;
    pkt->b2 = nonce;
    pkt->b3 = 0u;
    pkt->b4 = app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3);
}

bool packet_parse_pair_ack(const radio_packet_t *pkt, uint8_t *rx_id, uint8_t *nonce)
{
    if (pkt->b0 != 0xA5u) {
        return false;
    }
    if (pkt->b4 != app_checksum4(pkt->b0, pkt->b1, pkt->b2, pkt->b3)) {
        return false;
    }
    *rx_id = pkt->b1;
    *nonce = pkt->b2;
    return true;
}

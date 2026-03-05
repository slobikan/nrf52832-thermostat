#include "radio_protocol.h"
#include "nrf52832_regs.h"

#define RADIO_MODE_NRF_1MBIT 0u
#define RADIO_TXPOWER_0DBM   0x04u
#define RADIO_TIMEOUT_SPIN   200000u

static radio_packet_t g_radio_buffer;

static void wait_ready(volatile const uint32_t *event_reg) {
    while (!(*event_reg)) {
    }
}

void radio_init(void) {
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    wait_ready(&NRF_CLOCK->EVENTS_HFCLKSTARTED);

    NRF_RADIO->TASKS_DISABLE = 1;
    NRF_RADIO->EVENTS_DISABLED = 0;

    NRF_RADIO->MODE = RADIO_MODE_NRF_1MBIT;
    NRF_RADIO->FREQUENCY = 7;                 /* 2407 MHz */
    NRF_RADIO->TXPOWER = RADIO_TXPOWER_0DBM;
    NRF_RADIO->BASE0 = 0xE7E7E7E7u;
    NRF_RADIO->PREFIX0 = 0x000000E7u;
    NRF_RADIO->TXADDRESS = 0u;
    NRF_RADIO->RXADDRESSES = BIT(0);

    NRF_RADIO->PCNF0 = (8u << 0) | (0u << 8) | (0u << 16);
    NRF_RADIO->PCNF1 = (RADIO_PACKET_LEN << 0) | (RADIO_PACKET_LEN << 8) | (3u << 16);

    NRF_RADIO->CRCCNF = 2u;
    NRF_RADIO->CRCPOLY = 0x11021u;
    NRF_RADIO->CRCINIT = 0xFFFFu;

    NRF_RADIO->PACKETPTR = (uint32_t)(uintptr_t)&g_radio_buffer;
}

void radio_sleep(void) {
    NRF_RADIO->TASKS_DISABLE = 1;
    NRF_CLOCK->TASKS_HFCLKSTOP = 1;
}

void radio_wake(void) {
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    wait_ready(&NRF_CLOCK->EVENTS_HFCLKSTARTED);
}

uint8_t packet_checksum(const radio_packet_t *pkt) {
    const uint8_t *raw = (const uint8_t *)pkt;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < 6; i++) {
        sum ^= raw[i];
    }
    return (uint8_t)(sum ^ 0x5Au);
}

bool packet_is_valid(const radio_packet_t *pkt) {
    return (pkt->marker == 0xA5u) && (pkt->checksum == packet_checksum(pkt));
}

void packet_finalize(radio_packet_t *pkt) {
    pkt->checksum = packet_checksum(pkt);
    pkt->marker = 0xA5u;
}

bool radio_send_packet(radio_packet_t *pkt) {
    packet_finalize(pkt);
    g_radio_buffer = *pkt;

    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->TASKS_STARTTX = 1;
    wait_ready(&NRF_RADIO->EVENTS_READY);

    uint32_t guard = RADIO_TIMEOUT_SPIN;
    while (!NRF_RADIO->EVENTS_END && guard--) {
    }

    NRF_RADIO->TASKS_DISABLE = 1;
    return guard != 0u;
}

bool radio_recv_packet(radio_packet_t *pkt, uint32_t timeout_cycles) {
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->TASKS_STARTRX = 1;
    wait_ready(&NRF_RADIO->EVENTS_READY);

    while (!NRF_RADIO->EVENTS_END && timeout_cycles--) {
    }

    NRF_RADIO->TASKS_DISABLE = 1;
    if (timeout_cycles == 0u || NRF_RADIO->CRCSTATUS == 0u) {
        return false;
    }

    *pkt = g_radio_buffer;
    return packet_is_valid(pkt);
}

static void nvmc_wait_ready(void) {
    while (NRF_NVMC->READY == 0u) {
    }
}

bool pairing_store_device_id(uint8_t id) {
    uint32_t *page = (uint32_t *)PAIRING_FLASH_ADDR;

    NRF_NVMC->CONFIG = 2u;
    nvmc_wait_ready();
    NRF_NVMC->ERASEPAGE = PAIRING_FLASH_ADDR;
    nvmc_wait_ready();

    NRF_NVMC->CONFIG = 1u;
    nvmc_wait_ready();
    page[0] = 0x5448524Du; /* THRM */
    page[1] = id;
    nvmc_wait_ready();

    NRF_NVMC->CONFIG = 0u;
    return (page[0] == 0x5448524Du) && ((uint8_t)page[1] == id);
}

uint8_t pairing_load_device_id(void) {
    const uint32_t *page = (const uint32_t *)PAIRING_FLASH_ADDR;
    if (page[0] != 0x5448524Du) {
        return 0xFFu;
    }
    return (uint8_t)page[1];
}

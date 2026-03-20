#ifndef RADIO_LINK_H
#define RADIO_LINK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;
} radio_packet_t;

void radio_init(void);
void radio_power_down(void);

bool radio_tx_packet(const radio_packet_t *pkt, uint32_t timeout_ms);
bool radio_rx_packet(radio_packet_t *pkt, uint32_t timeout_ms, bool *crc_ok);
uint8_t radio_last_rssi_percent(void);

void packet_make_telemetry(radio_packet_t *pkt, uint16_t temp_x10, uint8_t setpoint, uint8_t tx_id);
bool packet_parse_telemetry(const radio_packet_t *pkt, uint16_t *temp_x10, uint8_t *setpoint, uint8_t *tx_id);

void packet_make_pair_req(radio_packet_t *pkt, uint8_t tx_id, uint8_t nonce);
bool packet_parse_pair_req(const radio_packet_t *pkt, uint8_t *tx_id, uint8_t *nonce);

void packet_make_pair_ack(radio_packet_t *pkt, uint8_t rx_id, uint8_t nonce);
bool packet_parse_pair_ack(const radio_packet_t *pkt, uint8_t *rx_id, uint8_t *nonce);

#endif

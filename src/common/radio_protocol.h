#ifndef RADIO_PROTOCOL_H
#define RADIO_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define RADIO_PACKET_LEN 8u
#define PAIRING_FLASH_ADDR 0x0007F000u

typedef enum {
    PKT_TYPE_TELEMETRY   = 0x10,
    PKT_TYPE_PAIR_REQ    = 0x20,
    PKT_TYPE_PAIR_ACK    = 0x21,
} packet_type_t;

typedef struct {
    uint8_t type;
    uint8_t device_id;
    uint8_t temperature;
    uint8_t setpoint;
    uint8_t seq;
    uint8_t flags;
    uint8_t checksum;
    uint8_t marker;
} radio_packet_t;

void radio_init(void);
void radio_sleep(void);
void radio_wake(void);

uint8_t packet_checksum(const radio_packet_t *pkt);
bool packet_is_valid(const radio_packet_t *pkt);
void packet_finalize(radio_packet_t *pkt);

bool radio_send_packet(radio_packet_t *pkt);
bool radio_recv_packet(radio_packet_t *pkt, uint32_t timeout_cycles);

bool pairing_store_device_id(uint8_t id);
uint8_t pairing_load_device_id(void);

#endif

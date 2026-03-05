#include <stdbool.h>
#include <stdint.h>

#include "../common/nrf52832_regs.h"
#include "../common/radio_protocol.h"

#define PIN_HEAT_OUT 24u

static void gpio_init_output(void) {
    NRF_P0->PIN_CNF[PIN_HEAT_OUT] = (1u << 0);
    NRF_P0->DIRSET = BIT(PIN_HEAT_OUT);
    NRF_P0->OUTCLR = BIT(PIN_HEAT_OUT);
}

static void heat_output_set(bool on) {
    if (on) {
        NRF_P0->OUTSET = BIT(PIN_HEAT_OUT);
    } else {
        NRF_P0->OUTCLR = BIT(PIN_HEAT_OUT);
    }
}

static bool pairing_handshake(uint8_t *bound_id) {
    radio_packet_t req = {
        .type = PKT_TYPE_PAIR_REQ,
        .seq = 0x3Cu,
        .flags = 0x55u,
    };
    radio_packet_t ack;

    for (uint8_t tries = 0; tries < 20; tries++) {
        req.seq++;
        if (!radio_send_packet(&req)) {
            continue;
        }

        if (!radio_recv_packet(&ack, 400000u)) {
            continue;
        }

        if (ack.type == PKT_TYPE_PAIR_ACK && ack.seq == req.seq) {
            *bound_id = ack.device_id;
            return pairing_store_device_id(*bound_id);
        }
    }

    return false;
}

int main(void) {
    radio_packet_t rx;
    uint8_t paired_id;

    gpio_init_output();
    radio_init();
    radio_wake();

    paired_id = pairing_load_device_id();
    if (paired_id == 0xFFu) {
        (void)pairing_handshake(&paired_id);
    }

    while (1) {
        if (!radio_recv_packet(&rx, 800000u)) {
            radio_sleep();
            radio_wake();
            continue;
        }

        if (rx.type != PKT_TYPE_TELEMETRY || rx.device_id != paired_id) {
            continue;
        }

        heat_output_set(rx.temperature < rx.setpoint);
    }
}

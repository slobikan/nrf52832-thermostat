#include <stdbool.h>
#include <stdint.h>

#include "../common/radio_protocol.h"

typedef enum {
    MODE_SHOW_TEMP = 0,
    MODE_SETPOINT,
    MODE_CO,
    MODE_PAIR,
    MODE_CONNECTED,
} ui_mode_t;

static volatile uint8_t g_temperature = 20;
static volatile uint8_t g_setpoint = 22;
static volatile ui_mode_t g_mode = MODE_SHOW_TEMP;

/* Provided by existing LCD/encoder/SAADC code. */
extern void ui_show_temperature(uint8_t temp);
extern void ui_show_setpoint_blink(uint8_t setpoint);
extern void ui_show_co_blink(void);
extern void ui_show_pair_blink(void);
extern void ui_show_connected_blink(void);
extern bool encoder_setpoint_changed(uint8_t *new_setpoint);
extern bool pair_button_pressed(void);
extern uint8_t sample_temperature_adc(void);
extern void delay_ms(uint32_t ms);

static uint8_t g_device_id;
static uint8_t g_tx_seq;

static void send_telemetry(bool immediate) {
    radio_packet_t pkt = {
        .type = PKT_TYPE_TELEMETRY,
        .device_id = g_device_id,
        .temperature = g_temperature,
        .setpoint = g_setpoint,
        .seq = g_tx_seq++,
        .flags = immediate ? 0x01u : 0x00u,
    };

    radio_wake();
    (void)radio_send_packet(&pkt);
    radio_sleep();
}

static bool try_pairing(void) {
    radio_packet_t rx;
    radio_packet_t tx = {0};

    ui_show_pair_blink();
    radio_wake();

    for (uint8_t attempt = 0; attempt < 15; attempt++) {
        if (!radio_recv_packet(&rx, 300000u)) {
            continue;
        }

        if (rx.type != PKT_TYPE_PAIR_REQ) {
            continue;
        }

        g_device_id = (uint8_t)(rx.seq ^ 0x5Cu);
        tx.type = PKT_TYPE_PAIR_ACK;
        tx.device_id = g_device_id;
        tx.temperature = g_temperature;
        tx.setpoint = g_setpoint;
        tx.seq = rx.seq;
        tx.flags = 0xA0u;

        if (radio_send_packet(&tx) && pairing_store_device_id(g_device_id)) {
            radio_sleep();
            ui_show_connected_blink();
            return true;
        }
    }

    radio_sleep();
    return false;
}

int main(void) {
    radio_init();
    g_device_id = pairing_load_device_id();
    if (g_device_id == 0xFFu) {
        g_device_id = 1u;
    }

    uint32_t measure_tick = 0;
    uint32_t periodic_tx_tick = 0;

    while (1) {
        if (pair_button_pressed()) {
            g_mode = MODE_PAIR;
            if (try_pairing()) {
                g_mode = MODE_CONNECTED;
                delay_ms(5000);
                g_mode = MODE_SHOW_TEMP;
            }
        }

        if (++measure_tick >= 20000u) {
            g_temperature = sample_temperature_adc();
            measure_tick = 0;
        }

        uint8_t new_sp;
        if (encoder_setpoint_changed(&new_sp)) {
            g_setpoint = (new_sp < 5u) ? 5u : (new_sp > 35u ? 35u : new_sp);
            g_mode = MODE_SETPOINT;
            ui_show_setpoint_blink(g_setpoint);
            delay_ms(10000);

            g_mode = MODE_CO;
            ui_show_co_blink();
            send_telemetry(true);
            delay_ms(5000);

            g_mode = MODE_SHOW_TEMP;
            periodic_tx_tick = 0;
        }

        if (++periodic_tx_tick >= 60000u) {
            send_telemetry(false);
            periodic_tx_tick = 0;
        }

        if (g_mode == MODE_SHOW_TEMP) {
            ui_show_temperature(g_temperature);
        }

        delay_ms(1);
    }
}

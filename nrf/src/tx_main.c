#include "common.h"
#include "nrf52832_regs.h"
#include "radio_link.h"
#include <stdbool.h>
#include <stdint.h>

#define COM1 26u
#define COM2 27u
#define COM3 28u
#define COM4 29u

#define SEG1 30u
#define SEG2 31u
#define SEG3 2u
#define SEG4 3u

#define SETPOINT_STORAGE_ADDR 0x0007D000u
#define SETPOINT_MAGIC        0x53505431u

#define NVMC_REG32(off)       (*((volatile uint32_t *)(NRF_NVMC_BASE + (off))))
#define NVMC_READY_OFF        0x400u
#define NVMC_CONFIG_OFF       0x504u
#define NVMC_ERASEPAGE_OFF    0x508u

#define TX_REQ_PERIOD_MS       80u
#define TX_ACK_SLICE_MS         8u
#define TX_ACK_WAIT_MS        220u
#define TX_TELEM_PERIOD_MS    500u
#define TX_BUTTON_DEBOUNCE_MS  25u
#define TX_SUCCESS_SHOW_MS   3000u
#define TX_BLINK_PERIOD_MS    500u
#define TX_LCD_SCAN_MS          2u
#define TX_TEMP_SAMPLE_MS   20000u
#define TX_PERIODIC_SEND_MS 60000u
#define TX_TEMP_MAX_C         50u
#define TX_SETPOINT_MIN_C     12u
#define TX_SETPOINT_MAX_C     35u
#define TX_ENC_ACCEPT_MS       50u
#define TX_SETPOINT_IDLE_MS  5000u
#define TX_CO_SHOW_MS        5000u
#define TX_CO_TX_DELAY_MS       0u
#define TX_CO_TX_BURST          3u
#define TX_CO_TX_GAP_MS        60u
#define TX_PERIODIC_TX_BURST    3u
#define TX_PERIODIC_TX_GAP_MS  40u
#define TX_IMMEDIATE_TX_BURST   2u
#define TX_IMMEDIATE_TX_GAP_MS 10u
#define TX_FAST_TX_WINDOW_MS 15000u
#define TX_FAST_TX_PERIOD_MS   500u
#define TX_LCD_DELAY_LOOPS   2000u

typedef enum {
    TX_MODE_SHOW_TEMP = 0,
    TX_MODE_SETPOINT,
    TX_MODE_CO,
    TX_MODE_PAIRING,
    TX_MODE_CONNECTED_HOLD
} tx_mode_t;

typedef struct {
    uint32_t magic;
    uint8_t setpoint;
    uint8_t reserved0;
    uint8_t reserved1;
    uint8_t checksum;
} setpoint_record_t;

enum {
    SEG_A = 1u << 0,
    SEG_B = 1u << 1,
    SEG_C = 1u << 2,
    SEG_D = 1u << 3,
    SEG_E = 1u << 4,
    SEG_F = 1u << 5,
    SEG_G = 1u << 6
};

static const uint8_t g_digit_masks[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,
    SEG_B | SEG_C | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
};

static uint8_t g_tx_id;
static uint8_t g_peer_rx_id;
static uint8_t g_com;
static uint8_t g_phase;
static uint8_t g_pair_nonce;
static uint16_t g_last_temp_x10 = 250u;
static uint8_t g_setpoint = 22u;
static bool g_blink_on = true;
static bool g_paired;
static bool g_setpoint_dirty;
static tx_mode_t g_mode = TX_MODE_SHOW_TEMP;
static uint32_t g_last_blink_ms;
static uint32_t g_last_lcd_scan_ms;
static uint32_t g_last_pair_tx_ms;
static uint32_t g_last_telem_ms;
static uint32_t g_last_temp_sample_ms;
static uint32_t g_last_periodic_tx_ms;
static uint32_t g_success_start_ms;
static uint32_t g_last_enc_ms;
static uint32_t g_last_enc_accept_ms;
static uint32_t g_co_start_ms;
static uint32_t g_fast_tx_until_ms;
static uint32_t g_next_fast_tx_ms;
static bool g_co_tx_done;
static bool g_setpoint_tx_sent;
static int8_t g_enc_accum;

static void lcd_delay(void)
{
    volatile uint32_t i;

    for (i = 0u; i < TX_LCD_DELAY_LOOPS; ++i) {
    }
}

static void lcd_config_pin(uint32_t pin)
{
    /* Input disconnected, no pull, standard drive, no sense.
     * This reduces leakage/ghosting on the passive LCD glass. */
    NRF_P0->PIN_CNF[pin] = 0x02u;
}

static void pin_high(uint32_t pin)
{
    NRF_P0->OUTSET = (1u << pin);
    NRF_P0->DIRSET = (1u << pin);
}

static void pin_low(uint32_t pin)
{
    NRF_P0->OUTCLR = (1u << pin);
    NRF_P0->DIRSET = (1u << pin);
}

static void pin_mid(uint32_t pin)
{
    NRF_P0->DIRCLR = (1u << pin);
}

static uint8_t setpoint_checksum(uint32_t magic, uint8_t setpoint)
{
    return (uint8_t)((magic & 0xFFu) ^
                     ((magic >> 8) & 0xFFu) ^
                     ((magic >> 16) & 0xFFu) ^
                     ((magic >> 24) & 0xFFu) ^
                     setpoint ^ 0x5Au);
}

static bool setpoint_load(uint8_t *setpoint)
{
    const setpoint_record_t *rec = (const setpoint_record_t *)SETPOINT_STORAGE_ADDR;

    if (rec->magic != SETPOINT_MAGIC) {
        return false;
    }
    if (rec->checksum != setpoint_checksum(rec->magic, rec->setpoint)) {
        return false;
    }
    if ((rec->setpoint < TX_SETPOINT_MIN_C) || (rec->setpoint > TX_SETPOINT_MAX_C)) {
        return false;
    }

    *setpoint = rec->setpoint;
    return true;
}

static void setpoint_store(uint8_t setpoint)
{
    setpoint_record_t rec;
    volatile uint32_t *dst = (volatile uint32_t *)SETPOINT_STORAGE_ADDR;

    rec.magic = SETPOINT_MAGIC;
    rec.setpoint = setpoint;
    rec.reserved0 = 0xFFu;
    rec.reserved1 = 0xFFu;
    rec.checksum = setpoint_checksum(rec.magic, rec.setpoint);

    NVMC_REG32(NVMC_CONFIG_OFF) = 2u;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }
    NVMC_REG32(NVMC_ERASEPAGE_OFF) = SETPOINT_STORAGE_ADDR;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }

    NVMC_REG32(NVMC_CONFIG_OFF) = 1u;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }

    dst[0] = rec.magic;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }
    dst[1] = ((uint32_t)rec.checksum << 24) |
             ((uint32_t)rec.reserved1 << 16) |
             ((uint32_t)rec.reserved0 << 8) |
             rec.setpoint;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }

    NVMC_REG32(NVMC_CONFIG_OFF) = 0u;
    while (NVMC_REG32(NVMC_READY_OFF) == 0u) {
    }
}

static void drive_com(uint8_t com)
{
    uint32_t pin = COM4;

    pin_mid(COM1);
    pin_mid(COM2);
    pin_mid(COM3);
    pin_mid(COM4);

    if (com == 0u) {
        pin = COM1;
    } else if (com == 1u) {
        pin = COM2;
    } else if (com == 2u) {
        pin = COM3;
    }

    if (g_phase != 0u) {
        pin_high(pin);
    } else {
        pin_low(pin);
    }
}

static void seg_all_mid(void)
{
    pin_mid(SEG1);
    pin_mid(SEG2);
    pin_mid(SEG3);
    pin_mid(SEG4);
}

static void seg_on(uint32_t seg)
{
    if (g_phase != 0u) {
        pin_low(seg);
    } else {
        pin_high(seg);
    }
}

static void lcd_show_mask_pair(uint8_t left_mask, uint8_t right_mask, bool show_degree)
{
    seg_all_mid();

    if (g_com == 0u) {
        if ((left_mask & SEG_A) != 0u) {
            seg_on(SEG1);
        }
        if ((right_mask & SEG_A) != 0u) {
            seg_on(SEG3);
        }
        if (show_degree) {
            seg_on(SEG2);
        }
    } else if (g_com == 1u) {
        if ((left_mask & SEG_F) != 0u) {
            seg_on(SEG1);
        }
        if ((left_mask & SEG_B) != 0u) {
            seg_on(SEG2);
        }
        if ((right_mask & SEG_F) != 0u) {
            seg_on(SEG3);
        }
        if ((right_mask & SEG_B) != 0u) {
            seg_on(SEG4);
        }
    } else if (g_com == 2u) {
        if ((left_mask & SEG_E) != 0u) {
            seg_on(SEG1);
        }
        if ((left_mask & SEG_G) != 0u) {
            seg_on(SEG2);
        }
        if ((right_mask & SEG_E) != 0u) {
            seg_on(SEG3);
        }
        if ((right_mask & SEG_G) != 0u) {
            seg_on(SEG4);
        }
    } else {
        if ((left_mask & SEG_D) != 0u) {
            seg_on(SEG1);
        }
        if ((left_mask & SEG_C) != 0u) {
            seg_on(SEG2);
        }
        if ((right_mask & SEG_D) != 0u) {
            seg_on(SEG3);
        }
        if ((right_mask & SEG_C) != 0u) {
            seg_on(SEG4);
        }
    }
}

static void lcd_show_digits(uint8_t left_digit, uint8_t right_digit, bool show_degree)
{
    uint8_t left_mask = (left_digit <= 9u) ? g_digit_masks[left_digit] : 0u;
    uint8_t right_mask = (right_digit <= 9u) ? g_digit_masks[right_digit] : 0u;

    lcd_show_mask_pair(left_mask, right_mask, show_degree);
}

static void lcd_show_pair_p(bool show_on)
{
    if (!show_on) {
        seg_all_mid();
        return;
    }

    seg_all_mid();

    if (g_com == 0u) {
        seg_on(SEG1);
    } else if (g_com == 1u) {
        seg_on(SEG1);
        seg_on(SEG2);
    } else if (g_com == 2u) {
        seg_on(SEG1);
        seg_on(SEG2);
    }
}

static void lcd_show_co(bool show_on)
{
    if (!show_on) {
        seg_all_mid();
        return;
    }

    seg_all_mid();

    if (g_com == 0u) {
        seg_on(SEG1);
        seg_on(SEG3);
    } else if (g_com == 1u) {
        seg_on(SEG1);
        seg_on(SEG3);
        seg_on(SEG4);
    } else if (g_com == 2u) {
        seg_on(SEG1);
        seg_on(SEG3);
    } else {
        seg_on(SEG1);
        seg_on(SEG3);
        seg_on(SEG4);
    }
}

static void lcd_init(void)
{
    lcd_config_pin(COM1);
    lcd_config_pin(COM2);
    lcd_config_pin(COM3);
    lcd_config_pin(COM4);
    lcd_config_pin(SEG1);
    lcd_config_pin(SEG2);
    lcd_config_pin(SEG3);
    lcd_config_pin(SEG4);

    pin_mid(COM1);
    pin_mid(COM2);
    pin_mid(COM3);
    pin_mid(COM4);
    seg_all_mid();
    g_com = 0u;
    g_phase = 0u;
    g_last_lcd_scan_ms = millis();
}

static void lcd_scan(void)
{
    drive_com(g_com);

    if (g_mode == TX_MODE_PAIRING) {
        /* Keep pairing indication solid so the display does not appear blank
         * while the user is trying to bind TX to RX. */
        lcd_show_pair_p(true);
    } else if (g_mode == TX_MODE_CO) {
        lcd_show_co(g_blink_on);
    } else if (g_mode == TX_MODE_CONNECTED_HOLD) {
        lcd_show_digits(9u, 9u, false);
    } else if (g_mode == TX_MODE_SETPOINT) {
        if (g_blink_on) {
            lcd_show_digits((uint8_t)(g_setpoint / 10u),
                            (uint8_t)(g_setpoint % 10u),
                            false);
        } else {
            seg_all_mid();
        }
    } else {
        {
            uint8_t display_temp = (uint8_t)(((uint32_t)g_last_temp_x10 + 5u) / 10u);

            lcd_show_digits((uint8_t)(display_temp / 10u),
                            (uint8_t)(display_temp % 10u),
                            true);
        }
    }

    ++g_com;
    if (g_com > 3u) {
        g_com = 0u;
        g_phase ^= 1u;
    }
}

static void ui_service(void)
{
    uint32_t now = millis();
    uint32_t scan_count = 0u;

    if ((now - g_last_blink_ms) >= TX_BLINK_PERIOD_MS) {
        g_last_blink_ms = now;
        g_blink_on = !g_blink_on;
    }

    while ((now - g_last_lcd_scan_ms) >= TX_LCD_SCAN_MS) {
        g_last_lcd_scan_ms += TX_LCD_SCAN_MS;
        lcd_scan();
        ++scan_count;

        /* Avoid spending too long catching up if radio/flash blocked us. */
        if (scan_count >= 4u) {
            g_last_lcd_scan_ms = now;
            break;
        }
    }
}

static void wait_with_ui(uint32_t duration_ms)
{
    uint32_t start = millis();

    while ((millis() - start) < duration_ms) {
        ui_service();
        lcd_delay();
    }
}

static bool pair_button_active(void)
{
    return !gpio_read(PIN_PAIR_BTN);
}

static bool pair_button_pressed(void)
{
    if (!pair_button_active()) {
        return false;
    }

    wait_with_ui(TX_BUTTON_DEBOUNCE_MS);
    return pair_button_active();
}

static uint16_t sample_temp_x10(void)
{
    uint32_t sum = 0u;
    uint32_t i;

    for (i = 0u; i < 16u; ++i) {
        int16_t raw = saadc_sample_raw();

        if (raw < 0) {
            raw = 0;
        }
        if (raw > 4095) {
            raw = 4095;
        }

        sum += (uint16_t)raw;
    }

    return (uint16_t)(((((sum >> 4) * (TX_TEMP_MAX_C * 10u))) + 2047u) / 4095u);
}

static int8_t encoder_poll_step(void)
{
    static uint8_t prev;
    uint8_t a = gpio_read(PIN_ENC_A) ? 1u : 0u;
    uint8_t b = gpio_read(PIN_ENC_B) ? 1u : 0u;
    uint8_t cur = (uint8_t)((a << 1) | b);
    int8_t step = 0;

    if (((prev == 0u) && (cur == 1u)) ||
        ((prev == 1u) && (cur == 3u)) ||
        ((prev == 3u) && (cur == 2u)) ||
        ((prev == 2u) && (cur == 0u))) {
        step = 1;
    } else if (((prev == 0u) && (cur == 2u)) ||
               ((prev == 2u) && (cur == 3u)) ||
               ((prev == 3u) && (cur == 1u)) ||
               ((prev == 1u) && (cur == 0u))) {
        step = -1;
    }

    prev = cur;
    return step;
}

static void lcd_resync_after_radio(void)
{
    uint32_t i;

    pin_mid(COM1);
    pin_mid(COM2);
    pin_mid(COM3);
    pin_mid(COM4);
    seg_all_mid();
    g_com = 0u;
    g_phase = 0u;

    for (i = 0u; i < 8u; ++i) {
        lcd_scan();
        lcd_delay();
    }

    g_last_lcd_scan_ms = millis();
}

static bool radio_tx_packet_lcd_friendly(const radio_packet_t *pkt, uint32_t timeout_ms)
{
    bool sent = radio_tx_packet(pkt, timeout_ms);

    radio_power_down();
    lcd_resync_after_radio();
    return sent;
}

static bool radio_rx_packet_lcd_friendly(radio_packet_t *pkt, uint32_t timeout_ms, bool *crc_ok)
{
    bool got = radio_rx_packet(pkt, timeout_ms, crc_ok);

    radio_power_down();
    lcd_resync_after_radio();
    return got;
}

static void send_telemetry_burst(uint32_t burst_count, uint32_t gap_ms)
{
    radio_packet_t pkt;
    uint32_t i;

    if (!g_paired) {
        return;
    }

    for (i = 0u; i < burst_count; ++i) {
        packet_make_telemetry(&pkt, g_last_temp_x10, g_setpoint, g_tx_id);
        (void)radio_tx_packet_lcd_friendly(&pkt, 20u);
        if ((i + 1u) < burst_count) {
            wait_with_ui(gap_ms);
        }
    }

    g_last_telem_ms = millis();
    g_last_periodic_tx_ms = g_last_telem_ms;
}

static void telemetry_send_if_due(void)
{
    /* Telemetry after setpoint changes is intentionally sent only
     * after the CO indication has finished. */
}

static void fast_update_send_if_due(void)
{
    /* Disabled: do not send before CO finishes. */
}

static void periodic_send_if_due(void)
{
    uint32_t now = millis();

    if (!g_paired) {
        return;
    }

    if ((g_mode != TX_MODE_SHOW_TEMP) && (g_mode != TX_MODE_CONNECTED_HOLD)) {
        return;
    }

    if ((now - g_last_periodic_tx_ms) < TX_PERIODIC_SEND_MS) {
        return;
    }

    send_telemetry_burst(TX_PERIODIC_TX_BURST, TX_PERIODIC_TX_GAP_MS);
    g_last_periodic_tx_ms = now;
}

static void update_temp_if_due(void)
{
    uint32_t now = millis();

    if ((now - g_last_temp_sample_ms) < TX_TEMP_SAMPLE_MS) {
        return;
    }

    g_last_temp_sample_ms = now;
    g_last_temp_x10 = sample_temp_x10();
}

static void start_pairing_mode(void)
{
    g_mode = TX_MODE_PAIRING;
    g_blink_on = true;
    g_last_blink_ms = millis();
    g_last_pair_tx_ms = 0u;
    g_pair_nonce = 0u;
    g_paired = false;
    g_peer_rx_id = 0u;
    (void)pair_record_clear();
}

static bool try_pair_exchange(void)
{
    uint32_t wait_start;
    radio_packet_t req;
    radio_packet_t ack;
    bool crc_ok = false;
    uint8_t rx_id = 0u;
    uint8_t ack_nonce = 0u;

    packet_make_pair_req(&req, g_tx_id, g_pair_nonce);
    (void)radio_tx_packet_lcd_friendly(&req, 20u);

    wait_start = millis();
    while ((millis() - wait_start) < TX_ACK_WAIT_MS) {
        if (radio_rx_packet_lcd_friendly(&ack, TX_ACK_SLICE_MS, &crc_ok) && crc_ok
                && packet_parse_pair_ack(&ack, &rx_id, &ack_nonce)
                && (ack_nonce == g_pair_nonce)) {
            g_peer_rx_id = rx_id;
            (void)pair_record_store(rx_id);
            g_paired = true;
            g_mode = TX_MODE_CONNECTED_HOLD;
            g_success_start_ms = millis();
            g_last_temp_x10 = sample_temp_x10();
            g_last_temp_sample_ms = g_success_start_ms;
            send_telemetry_burst(TX_IMMEDIATE_TX_BURST, TX_IMMEDIATE_TX_GAP_MS);
            return true;
        }

        ui_service();
        lcd_delay();
    }

    ++g_pair_nonce;
    return false;
}

int main(void)
{
    pair_record_t rec;
    uint8_t stored_setpoint;

    clock_hf_start();
    timer0_init_1mhz();
    saadc_init_ain2_p04();
    radio_init();
    lcd_init();
    gpio_input_pullup(PIN_PAIR_BTN);
    gpio_input_pullup(PIN_ENC_A);
    gpio_input_pullup(PIN_ENC_B);

    if (setpoint_load(&stored_setpoint)) {
        g_setpoint = stored_setpoint;
    }

    g_tx_id = device_unique_id8();
    g_last_temp_x10 = sample_temp_x10();
    g_last_blink_ms = millis();
    g_last_telem_ms = g_last_blink_ms;
    g_last_temp_sample_ms = g_last_blink_ms;
    g_last_periodic_tx_ms = g_last_blink_ms;
    g_last_enc_ms = g_last_blink_ms;
    g_last_enc_accept_ms = g_last_blink_ms;

    if (pair_record_load(&rec)) {
        g_peer_rx_id = rec.peer_id;
        g_paired = true;
    }

    while (1) {
        uint32_t now;
        int8_t enc_step;

        ui_service();
        now = millis();

        if ((g_mode != TX_MODE_PAIRING) &&
                (g_mode != TX_MODE_CONNECTED_HOLD) &&
                (g_mode != TX_MODE_CO)) {
            int8_t enc_q = encoder_poll_step();

            if (enc_q != 0) {
                g_enc_accum += enc_q;
                if (g_enc_accum >= 4) {
                    enc_step = 1;
                    g_enc_accum = 0;
                } else if (g_enc_accum <= -4) {
                    enc_step = -1;
                    g_enc_accum = 0;
                } else {
                    enc_step = 0;
                }

                if ((enc_step != 0) && ((now - g_last_enc_accept_ms) >= TX_ENC_ACCEPT_MS)) {
                    g_last_enc_accept_ms = now;

                    if (enc_step > 0) {
                        if (g_setpoint > TX_SETPOINT_MIN_C) {
                            --g_setpoint;
                        }
                    } else {
                        if (g_setpoint < TX_SETPOINT_MAX_C) {
                            ++g_setpoint;
                        }
                    }

                    g_mode = TX_MODE_SETPOINT;
                    g_setpoint_dirty = true;
                    g_blink_on = true;
                    g_last_blink_ms = now;
                    g_last_enc_ms = now;
                    g_fast_tx_until_ms = 0u;
                    g_next_fast_tx_ms = 0u;
                    g_setpoint_tx_sent = false;
                }
            }
        }

        if ((g_mode != TX_MODE_PAIRING) && pair_button_pressed()) {
            start_pairing_mode();
            while (pair_button_active()) {
                ui_service();
                lcd_delay();
            }
        }

        if (g_mode == TX_MODE_PAIRING) {
            now = millis();
            if ((now - g_last_pair_tx_ms) >= TX_REQ_PERIOD_MS) {
                g_last_pair_tx_ms = now;
                (void)try_pair_exchange();
            }
            lcd_delay();
            continue;
        }

        update_temp_if_due();
        fast_update_send_if_due();

        if ((g_mode == TX_MODE_SETPOINT) && ((millis() - g_last_enc_ms) >= TX_SETPOINT_IDLE_MS)) {
            if (g_setpoint_dirty) {
                setpoint_store(g_setpoint);
                g_setpoint_dirty = false;
            }
            g_mode = TX_MODE_CO;
            g_blink_on = true;
            g_last_blink_ms = millis();
            g_co_start_ms = g_last_blink_ms;
            g_co_tx_done = false;
            g_setpoint_tx_sent = false;
            g_last_telem_ms = g_co_start_ms;
        }

        telemetry_send_if_due();
        periodic_send_if_due();

        if ((g_mode == TX_MODE_CO) &&
                ((millis() - g_co_start_ms) >= TX_CO_SHOW_MS)) {
            if (!g_co_tx_done) {
                send_telemetry_burst(TX_CO_TX_BURST, TX_CO_TX_GAP_MS);
                g_co_tx_done = true;
            }
            g_mode = TX_MODE_SHOW_TEMP;
            g_blink_on = true;
            g_last_blink_ms = millis();
        }

        if ((g_mode == TX_MODE_CONNECTED_HOLD)
                && ((millis() - g_success_start_ms) >= TX_SUCCESS_SHOW_MS)) {
            g_mode = TX_MODE_SHOW_TEMP;
        }

        lcd_delay();
    }
}

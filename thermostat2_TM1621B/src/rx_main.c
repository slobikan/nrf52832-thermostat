#include "common.h"
#include "radio_link.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define RX_BUTTON_DEBOUNCE_MS  25u
#define RX_PAIR_WINDOW_MS     180u
#define RX_ACK_REPEAT           8u
#define RX_ACK_DELAY_MS        10u
#define RX_TELEM_WINDOW_MS     700u
#define RX_TELEM_SLICE_MS       25u
#define RX_LINK_TIMEOUT_MS   3000u
#define RX_UART_HEARTBEAT_MS 1000u
#define RX_UART_BUF_LEN        64u
#define RX_ZONE_MAX            15u

#define UART0_BASE                 0x40002000u
#define UART_TASKS_STARTRX_OFF     0x000u
#define UART_REG32(off)            (*((volatile uint32_t *)(UART0_BASE + (off))))
#define UART_TASKS_STARTTX_OFF     0x008u
#define UART_EVENTS_RXDRDY_OFF     0x108u
#define UART_EVENTS_TXDRDY_OFF     0x11Cu
#define UART_ENABLE_OFF            0x500u
#define UART_PSELTXD_OFF           0x50Cu
#define UART_PSELRXD_OFF           0x514u
#define UART_RXD_OFF               0x518u
#define UART_TXD_OFF               0x51Cu
#define UART_BAUDRATE_OFF          0x524u
#define UART_CONFIG_OFF            0x56Cu
#define UART_ENABLE_ENABLED        4u
#define UART_BAUDRATE_115200       0x01D7E000u

static uint8_t g_rx_id;
static uint8_t g_paired_tx_id;
static uint16_t g_last_temp_x10;
static uint8_t g_last_setpoint;
static uint8_t g_last_rssi_percent;
static uint8_t g_zone_id;
static uint8_t g_requested_zone_id;
static bool g_link_up;
static bool g_pair_request_pending;
static uint8_t g_cmd_setpoint;
static bool g_cmd_setpoint_valid;
typedef enum {
    CTRL_MODE_FOLLOW = 0,
    CTRL_MODE_COOL,
    CTRL_MODE_HEAT,
    CTRL_MODE_AUTO,
    CTRL_MODE_FAN,
    CTRL_MODE_OFF
} ctrl_mode_t;
static ctrl_mode_t g_ctrl_mode;
static char g_uart_rx_buf[RX_UART_BUF_LEN];
static uint8_t g_uart_rx_len;
static uint32_t g_last_uart_heartbeat_ms;

static void clear_pair_state(void);
static void uart_publish_status(void);
static void uart_publish_heartbeat_if_due(void);

static bool should_enable_heat(uint16_t temp_x10, uint8_t setpoint)
{
    return (temp_x10 + 10u) <= ((uint16_t)setpoint * 10u);
}

static uint8_t effective_setpoint(void)
{
    return g_cmd_setpoint_valid ? g_cmd_setpoint : g_last_setpoint;
}

static bool should_enable_output(uint16_t temp_x10, uint8_t setpoint)
{
    switch (g_ctrl_mode) {
    case CTRL_MODE_COOL:
        if ((temp_x10 == 0u) || (setpoint == 0u)) {
            return false;
        }
        return temp_x10 > ((uint16_t)setpoint * 10u);
    case CTRL_MODE_HEAT:
        return should_enable_heat(temp_x10, setpoint);
    case CTRL_MODE_AUTO:
        if ((temp_x10 == 0u) || (setpoint == 0u)) {
            return false;
        }
        return (temp_x10 > ((uint16_t)setpoint * 10u)) || should_enable_heat(temp_x10, setpoint);
    case CTRL_MODE_FAN:
        return true;
    case CTRL_MODE_OFF:
        return false;
    case CTRL_MODE_FOLLOW:
    default:
        return should_enable_heat(temp_x10, setpoint);
    }
}

static void apply_output_state(void)
{
    if (should_enable_output(g_last_temp_x10, effective_setpoint())) {
        gpio_set(PIN_RX_OUT);
    } else {
        gpio_clear(PIN_RX_OUT);
    }
}

static uint8_t parse_temp_value(const char *value)
{
    uint32_t temp = 0u;

    while ((*value >= '0') && (*value <= '9')) {
        temp = temp * 10u + (uint32_t)(*value - '0');
        ++value;
    }

    if (temp > 99u) {
        temp = 99u;
    }
    return (uint8_t)temp;
}

static void set_status_output(bool on)
{
    if (on) {
        gpio_set(PIN_RX_STATUS);
    } else {
        gpio_clear(PIN_RX_STATUS);
    }
}

static bool pair_button_active(void)
{
    return !gpio_read(PIN_RX_PAIR_IN);
}

static bool pair_button_pressed(void)
{
    if (!pair_button_active()) {
        return false;
    }

    delay_ms(RX_BUTTON_DEBOUNCE_MS);
    return pair_button_active();
}

static void uart_init(void)
{
    UART_REG32(UART_ENABLE_OFF) = 0u;
    UART_REG32(UART_PSELTXD_OFF) = PIN_RX_UART_TX;
    UART_REG32(UART_PSELRXD_OFF) = PIN_RX_UART_RX;
    UART_REG32(UART_CONFIG_OFF) = 0u;
    UART_REG32(UART_BAUDRATE_OFF) = UART_BAUDRATE_115200;
    UART_REG32(UART_ENABLE_OFF) = UART_ENABLE_ENABLED;
    UART_REG32(UART_EVENTS_RXDRDY_OFF) = 0u;
    UART_REG32(UART_EVENTS_TXDRDY_OFF) = 0u;
    UART_REG32(UART_TASKS_STARTRX_OFF) = 1u;
    UART_REG32(UART_TASKS_STARTTX_OFF) = 1u;
}

static void uart_write_byte(uint8_t byte)
{
    UART_REG32(UART_EVENTS_TXDRDY_OFF) = 0u;
    UART_REG32(UART_TXD_OFF) = byte;
    while (UART_REG32(UART_EVENTS_TXDRDY_OFF) == 0u) {
    }
}

static void uart_write_text(const char *text)
{
    while (*text != '\0') {
        uart_write_byte((uint8_t)*text++);
    }
}

static void uart_write_line2(const char *prefix, const char *text)
{
    uart_write_text(prefix);
    uart_write_text(text);
    uart_write_text("\n");
}

static char *append_text(char *dst, const char *text)
{
    while (*text != '\0') {
        *dst++ = *text++;
    }
    return dst;
}

static char *append_uint_dec(char *dst, uint32_t value)
{
    char tmp[10];
    uint32_t len = 0u;

    do {
        tmp[len++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (len > 0u) {
        *dst++ = tmp[--len];
    }
    return dst;
}

static char *append_temp_x10(char *dst, uint16_t temp_x10)
{
    dst = append_uint_dec(dst, (uint32_t)(temp_x10 / 10u));
    *dst++ = '.';
    *dst++ = (char)('0' + (temp_x10 % 10u));
    return dst;
}

static void uart_publish_status(void)
{
    char line[96];
    char *p = line;

    p = append_text(p, "ZONE=");
    p = append_uint_dec(p, g_zone_id);
    p = append_text(p, ";ID=");
    p = append_uint_dec(p, g_paired_tx_id);
    p = append_text(p, ";LINK=");
    p = append_uint_dec(p, g_link_up ? 1u : 0u);
    p = append_text(p, ";TEMP=");
    p = append_temp_x10(p, g_last_temp_x10);
    p = append_text(p, ";TARGET=");
    p = append_uint_dec(p, effective_setpoint());
    p = append_text(p, ";THERMO_TARGET=");
    p = append_uint_dec(p, g_last_setpoint);
    p = append_text(p, ";RSSI=");
    p = append_uint_dec(p, g_last_rssi_percent);
    *p++ = '\n';
    *p = '\0';

    uart_write_text(line);
}

static void uart_publish_heartbeat_if_due(void)
{
    uint32_t now = millis();

    if ((now - g_last_uart_heartbeat_ms) < RX_UART_HEARTBEAT_MS) {
        return;
    }

    g_last_uart_heartbeat_ms = now;
    uart_write_text("HB;");
    uart_publish_status();
}

static uint8_t parse_zone_value(const char *value)
{
    uint32_t zone = 0u;

    while ((*value >= '0') && (*value <= '9')) {
        zone = zone * 10u + (uint32_t)(*value - '0');
        ++value;
    }

    if ((zone == 0u) || (zone > RX_ZONE_MAX)) {
        return 0u;
    }

    return (uint8_t)zone;
}

static void handle_uart_command(const char *line)
{
    char work[RX_UART_BUF_LEN];
    char *token;
    uint8_t pending_zone = 0u;

    for (uint32_t i = 0u; i < RX_UART_BUF_LEN; ++i) {
        work[i] = line[i];
        if (line[i] == '\0') {
            break;
        }
    }
    work[RX_UART_BUF_LEN - 1u] = '\0';
    uart_write_line2("ECHO:", line);

    for (token = work; *token != '\0'; ) {
        char *next = token;
        char *sep;

        while ((*next != '\0') && (*next != ';')) {
            ++next;
        }
        if (*next == ';') {
            *next++ = '\0';
        }

        sep = token;
        while ((*sep != '\0') && (*sep != '=')) {
            ++sep;
        }
        if (*sep == '=') {
            uint8_t zone;

            *sep++ = '\0';
            zone = parse_zone_value(sep);

            if (((token[0] == 'S') || (token[0] == 's') ||
                 (token[0] == 'P') || (token[0] == 'p')) && (zone != 0u)) {
                pending_zone = zone;
                g_requested_zone_id = zone;
                g_pair_request_pending = true;
            } else if (((token[0] == 'U') || (token[0] == 'u')) && (zone != 0u)) {
                if ((g_zone_id == zone) || (g_zone_id == 0u)) {
                    g_requested_zone_id = zone;
                    g_pair_request_pending = false;
                    g_zone_id = 0u;
                    clear_pair_state();
                }
            } else if ((strcmp(token, "TARGET") == 0) || (strcmp(token, "SETPOINT") == 0)) {
                g_cmd_setpoint = parse_temp_value(sep);
                g_cmd_setpoint_valid = (g_cmd_setpoint != 0u);
                apply_output_state();
                uart_publish_status();
            } else if (strcmp(token, "MODE") == 0) {
                if ((sep[0] == 'c') || (sep[0] == 'C')) {
                    g_ctrl_mode = CTRL_MODE_COOL;
                } else if ((sep[0] == 'h') || (sep[0] == 'H')) {
                    g_ctrl_mode = CTRL_MODE_HEAT;
                } else if ((sep[0] == 'a') || (sep[0] == 'A')) {
                    g_ctrl_mode = CTRL_MODE_AUTO;
                } else if ((sep[0] == 'f') || (sep[0] == 'F')) {
                    g_ctrl_mode = CTRL_MODE_FAN;
                } else if ((sep[0] == 'o') || (sep[0] == 'O')) {
                    g_ctrl_mode = CTRL_MODE_OFF;
                } else {
                    g_ctrl_mode = CTRL_MODE_FOLLOW;
                }
                apply_output_state();
                uart_publish_status();
            }
        }

        token = next;
    }

    if ((pending_zone != 0u) && (g_zone_id != pending_zone)) {
        g_zone_id = pending_zone;
        clear_pair_state();
    }
}

static void uart_poll_commands(void)
{
    while (UART_REG32(UART_EVENTS_RXDRDY_OFF) != 0u) {
        uint8_t byte = (uint8_t)UART_REG32(UART_RXD_OFF);
        UART_REG32(UART_EVENTS_RXDRDY_OFF) = 0u;

        if ((byte == '\r') || (byte == '\n')) {
            if (g_uart_rx_len > 0u) {
                g_uart_rx_buf[g_uart_rx_len] = '\0';
                handle_uart_command(g_uart_rx_buf);
                g_uart_rx_len = 0u;
            }
        } else if (g_uart_rx_len < (RX_UART_BUF_LEN - 1u)) {
            g_uart_rx_buf[g_uart_rx_len++] = (char)byte;
        } else {
            g_uart_rx_len = 0u;
        }
    }
}

static void start_pairing_request(uint8_t zone_id)
{
    g_requested_zone_id = zone_id;
    g_pair_request_pending = true;
}

static void clear_pair_state(void)
{
    (void)pair_record_clear();
    g_paired_tx_id = 0u;
    g_last_temp_x10 = 0u;
    g_last_setpoint = 0u;
    g_last_rssi_percent = 0u;
    g_link_up = false;
    g_cmd_setpoint = 0u;
    g_cmd_setpoint_valid = false;
    g_ctrl_mode = CTRL_MODE_FOLLOW;
    set_status_output(false);
    gpio_clear(PIN_RX_OUT);
    uart_publish_status();
}

static bool pair_wait_and_ack(void)
{
    while (1) {
        radio_packet_t pkt;
        bool crc_ok = false;
        uint8_t tx_id = 0u;
        uint8_t nonce = 0u;

        uart_poll_commands();
        uart_publish_heartbeat_if_due();

        if (radio_rx_packet(&pkt, RX_PAIR_WINDOW_MS, &crc_ok) && crc_ok
                && packet_parse_pair_req(&pkt, &tx_id, &nonce)) {
            radio_packet_t ack;

            packet_make_pair_ack(&ack, g_rx_id, nonce);

            for (uint32_t i = 0u; i < RX_ACK_REPEAT; ++i) {
                (void)radio_tx_packet(&ack, 20u);
                if ((i + 1u) < RX_ACK_REPEAT) {
                    delay_ms(RX_ACK_DELAY_MS);
                }
            }

            radio_power_down();

            g_paired_tx_id = tx_id;
            g_zone_id = g_requested_zone_id;
            g_pair_request_pending = false;
            (void)pair_record_store_zone(tx_id, g_zone_id);
            set_status_output(true);
    g_last_temp_x10 = 0u;
            g_last_setpoint = 0u;
            g_last_rssi_percent = 0u;
            g_link_up = false;
            uart_publish_status();
            return true;
        }

        radio_power_down();

        if (pair_button_active()) {
            return false;
        }

        if (!g_pair_request_pending) {
            return false;
        }
    }
}

static bool run_receive(void)
{
    uint32_t last_rx_ms = millis();

    while (1) {
        radio_packet_t pkt;
        bool crc_ok = false;
        uint16_t temp_x10 = 0u;
        uint8_t setpoint = 0u;
        uint8_t tx_id = 0u;

        uart_poll_commands();
        uart_publish_heartbeat_if_due();

        if (pair_button_active()) {
            gpio_clear(PIN_RX_OUT);
            return true;
        }

        if (g_pair_request_pending) {
            return true;
        }

        if (radio_rx_packet(&pkt, RX_TELEM_SLICE_MS, &crc_ok) && crc_ok
                && packet_parse_telemetry(&pkt, &temp_x10, &setpoint, &tx_id)
                && (tx_id == g_paired_tx_id)) {
            last_rx_ms = millis();
            set_status_output(true);
            g_last_temp_x10 = temp_x10;
            g_last_setpoint = setpoint;
            g_last_rssi_percent = radio_last_rssi_percent();
            g_link_up = true;
            apply_output_state();
            uart_publish_status();
        }

        radio_power_down();

        if ((millis() - last_rx_ms) > RX_LINK_TIMEOUT_MS) {
            if (g_link_up) {
                g_link_up = false;
                uart_publish_status();
            }
            set_status_output(false);
        }
    }
}

int main(void)
{
    bool paired = false;

    clock_hf_start();
    timer0_init_1mhz();
    radio_init();
    uart_init();

    gpio_output(PIN_RX_STATUS);
    gpio_output(PIN_RX_OUT);
    gpio_input_pullup(PIN_RX_PAIR_IN);

    set_status_output(false);
    gpio_clear(PIN_RX_OUT);

    g_rx_id = device_unique_id8();

    {
        pair_record_t rec;
        if (pair_record_load(&rec)) {
            g_paired_tx_id = rec.peer_id;
            g_zone_id = (rec.reserved0 <= RX_ZONE_MAX) ? rec.reserved0 : 0u;
            paired = true;
        }
    }

    uart_publish_status();

    while (1) {
        uart_poll_commands();
        uart_publish_heartbeat_if_due();

        if (pair_button_pressed()) {
            start_pairing_request(g_zone_id);
            clear_pair_state();

            while (pair_button_active()) {
                delay_ms(10u);
            }

            paired = pair_wait_and_ack();
            continue;
        }

        if (g_pair_request_pending) {
            clear_pair_state();
            paired = pair_wait_and_ack();
            continue;
        }

        if (paired) {
            if (run_receive()) {
                clear_pair_state();
                paired = false;
            }
            continue;
        }

        delay_ms(10u);
    }
}


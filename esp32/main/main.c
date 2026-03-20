#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#ifndef CONFIG_AJAX_WIFI_SSID
#define CONFIG_AJAX_WIFI_SSID "SLOBIKI"
#endif

#ifndef CONFIG_AJAX_WIFI_PASSWORD
#define CONFIG_AJAX_WIFI_PASSWORD "440123824"
#endif

#ifndef CONFIG_AJAX_MQTT_URI
#define CONFIG_AJAX_MQTT_URI "mqtt://192.168.0.249:1883"
#endif

#ifndef CONFIG_AJAX_MQTT_USERNAME
#define CONFIG_AJAX_MQTT_USERNAME ""
#endif

#ifndef CONFIG_AJAX_MQTT_PASSWORD
#define CONFIG_AJAX_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_AJAX_HA_DISCOVERY_PREFIX
#define CONFIG_AJAX_HA_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef CONFIG_AJAX_DEVICE_NAME
#define CONFIG_AJAX_DEVICE_NAME "AJAX KLIM"
#endif

#ifndef CONFIG_AJAX_NODE_ID
#define CONFIG_AJAX_NODE_ID "ajax_klim_esp32"
#endif

#ifndef CONFIG_AJAX_ENABLE_UART
#define CONFIG_AJAX_ENABLE_UART 1
#endif

#ifndef CONFIG_AJAX_UART_BAUD
#define CONFIG_AJAX_UART_BAUD 115200
#endif

#ifndef CONFIG_AJAX_ENABLE_ETHERNET
#define CONFIG_AJAX_ENABLE_ETHERNET 0
#endif

#ifndef CONFIG_AJAX_ETH_SPI_CLOCK_MHZ
#define CONFIG_AJAX_ETH_SPI_CLOCK_MHZ 12
#endif

#ifndef CONFIG_AJAX_ETH_SPI_POLL_MS
#define CONFIG_AJAX_ETH_SPI_POLL_MS 10
#endif

#define PIN_TFT_BL    GPIO_NUM_32
#define PIN_TFT_CS    GPIO_NUM_33
#define PIN_TFT_DC    GPIO_NUM_25
#define PIN_TFT_RST   GPIO_NUM_26
#define PIN_TFT_MOSI  GPIO_NUM_27
#define PIN_TFT_SCK   GPIO_NUM_14
#define PIN_UART_RX   GPIO_NUM_16
#define PIN_UART_TX   GPIO_NUM_17
#define PIN_ETH_MISO  GPIO_NUM_19
#define PIN_ETH_CS    GPIO_NUM_21
#define PIN_ETH_RST   GPIO_NUM_22
#define PIN_ETH_INT   (-1)
#define PIN_BTN_UP    GPIO_NUM_4
#define PIN_BTN_DOWN  GPIO_NUM_13
#define PIN_BTN_MENU  GPIO_NUM_18
#define PIN_BTN_EXIT  GPIO_NUM_23
#define PIN_RTC_CE    GPIO_NUM_15
#define PIN_RTC_IO    GPIO_NUM_2
#define PIN_RTC_SCLK  GPIO_NUM_5

#define LCD_HOST      SPI2_HOST
#define LCD_WIDTH     128
#define LCD_HEIGHT    160
#define LCD_SPI_HZ    (10000000)

#define UART_PORT_NUM            UART_NUM_2
#define UART_RX_BUFFER_SIZE      256
#define UART_TASK_STACK_SIZE     4096
#define WIFI_CONNECTED_BIT       BIT0
#define HVAC_MIN_TEMP_C          16.0f
#define HVAC_MAX_TEMP_C          30.0f
#define AUTO_HYSTERESIS_DEFAULT_C 1.0f
#define TEMP_CALIBRATION_DEFAULT_C 0.0f
#define HYSTERESIS_MIN_C         0.0f
#define HYSTERESIS_MAX_C         10.0f
#define HYSTERESIS_STEP_C        0.5f
#define CALIBRATION_MIN_C       -10.0f
#define CALIBRATION_MAX_C        10.0f
#define CALIBRATION_STEP_C        0.1f
#define ZONE_MAX_COUNT           15u
#define ZONE_ACTIVE_COUNT        8u
#define DASHBOARD_ZONE_SLOTS     8u
#define STATE_PUBLISH_PERIOD_MS  30000
#define BOOT_SPLASH_DURATION_MS  10000
#define BOOT_SPLASH_STEP_MS      200
#define UI_BUTTON_POLL_MS        30
#define UI_BUTTON_HOLD_OFF_MS    180
#define UI_LOGICAL_WIDTH         LCD_HEIGHT
#define UI_LOGICAL_HEIGHT        LCD_WIDTH
#define ZONE_SYNC_WAIT_MS        120000
#define ZONE_SYNC_ANIM_PERIOD_MS 1600
#define ZONE_SYNC_RETRY_MS       250
#define TARGET_OVERRIDE_RETRY_MS 500
#define TARGET_OVERRIDE_TIMEOUT_MS 10000
#define AC_START_DELAY_DEFAULT_S 30u
#define AC_START_DELAY_MIN_S     5u
#define AC_START_DELAY_MAX_S     120u
#define AUX_RELAY_DELAY_DEFAULT_S 0u
#define AUX_RELAY_DELAY_MIN_S     0u
#define AUX_RELAY_DELAY_MAX_S     60u
#define TARGET_TEMP_DEFAULT_C     24.0f
#define DS1302_IO_DELAY_US         1u

#define CMD_SWRESET   0x01
#define CMD_SLPOUT    0x11
#define CMD_DISPON    0x29
#define CMD_CASET     0x2A
#define CMD_RASET     0x2B
#define CMD_RAMWR     0x2C
#define CMD_MADCTL    0x36
#define CMD_COLMOD    0x3A
#define CMD_NORON     0x13
#define CMD_INVOFF    0x20
#define CMD_INVON     0x21
#define CMD_FRMCTR1   0xB1
#define CMD_FRMCTR2   0xB2
#define CMD_FRMCTR3   0xB3
#define CMD_INVCTR    0xB4
#define CMD_PWCTR1    0xC0
#define CMD_PWCTR2    0xC1
#define CMD_PWCTR3    0xC2
#define CMD_PWCTR4    0xC3
#define CMD_PWCTR5    0xC4
#define CMD_VMCTR1    0xC5
#define CMD_GMCTRP1   0xE0
#define CMD_GMCTRN1   0xE1

static const char *TAG = "ajax_klim";
static spi_device_handle_t s_lcd;
static EventGroupHandle_t s_wifi_event_group;
static SemaphoreHandle_t s_state_mutex;
static SemaphoreHandle_t s_uart_mutex;
static esp_mqtt_client_handle_t s_mqtt_client;
static esp_eth_handle_t s_eth_handle;
static esp_netif_t *s_eth_netif;
static SemaphoreHandle_t s_ui_mutex;
static bool s_discovery_sent;
static uint16_t s_lcd_framebuffer[LCD_WIDTH * LCD_HEIGHT];
static time_t s_clock_base_epoch;
static int64_t s_clock_base_us;
static struct tm s_clock_edit_tm;
static bool s_target_override_pending;
static float s_target_override_pending_c;
static int64_t s_target_override_deadline_ms;
static int64_t s_target_override_last_send_ms;

typedef enum {
    HVAC_MODE_OFF = 0,
    HVAC_MODE_COOL,
    HVAC_MODE_HEAT,
    HVAC_MODE_FAN_ONLY,
    HVAC_MODE_AUTO
} hvac_mode_t;

typedef enum {
    UI_SCREEN_BOOT = 0,
    UI_SCREEN_HOME,
    UI_SCREEN_WORK_MODE,
    UI_SCREEN_ZONES,
    UI_SCREEN_ZONE_DETAIL,
    UI_SCREEN_ZONE_CONNECTION,
    UI_SCREEN_ZONE_SYNC_CONFIRM,
    UI_SCREEN_ZONE_SYNC_WAIT,
    UI_SCREEN_ZONE_TEMP_SOURCE,
    UI_SCREEN_ZONE_CHRONOGRAM,
    UI_SCREEN_SYSTEM,
    UI_SCREEN_SYSTEM_TIME,
    UI_SCREEN_SYSTEM_DATE_SETUP,
    UI_SCREEN_SYSTEM_TIME_SETUP,
    UI_SCREEN_SYSTEM_DISPLAY,
    UI_SCREEN_SYSTEM_LANGUAGE,
    UI_SCREEN_INSTALLER,
    UI_SCREEN_INSTALLER_ROOM_THERMOSTAT,
    UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS,
    UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION,
    UI_SCREEN_INSTALLER_IO_CONFIG,
    UI_SCREEN_INSTALLER_IO_ZONE_RELAYS,
    UI_SCREEN_INSTALLER_IO_DRY_CONTACT_RELAY,
    UI_SCREEN_INSTALLER_IO_ZONE_AUX_SENSORS,
    UI_SCREEN_INSTALLER_NETWORK_SETUP,
    UI_SCREEN_INSTALLER_NETWORK_DHCP,
    UI_SCREEN_INSTALLER_NETWORK_IP_ADDRESS,
    UI_SCREEN_INSTALLER_NETWORK_SUBNET_MASK,
    UI_SCREEN_INSTALLER_NETWORK_GATEWAY,
    UI_SCREEN_INSTALLER_NETWORK_DNS,
    UI_SCREEN_INSTALLER_NETWORK_HOSTNAME,
    UI_SCREEN_INSTALLER_NETWORK_LINK_STATUS,
    UI_SCREEN_INSTALLER_NETWORK_MAC_ADDRESS,
    UI_SCREEN_INSTALLER_WIFI_SETUP,
    UI_SCREEN_INSTALLER_WIFI_ENABLE,
    UI_SCREEN_INSTALLER_WIFI_SSID,
    UI_SCREEN_INSTALLER_WIFI_PASSWORD,
    UI_SCREEN_INSTALLER_WIFI_DHCP,
    UI_SCREEN_INSTALLER_WIFI_IP_ADDRESS,
    UI_SCREEN_INSTALLER_WIFI_GATEWAY,
    UI_SCREEN_INSTALLER_WIFI_STATUS,
    UI_SCREEN_INSTALLER_WIFI_SIGNAL,
    UI_SCREEN_INSTALLER_MANUAL_CONTROL,
    UI_SCREEN_INSTALLER_OUTDOOR_SENSOR,
    UI_SCREEN_INSTALLER_DELAY,
    UI_SCREEN_INSTALLER_FACTORY_RESET,
    UI_SCREEN_SERVICE
} ui_screen_t;

typedef struct {
    ui_screen_t screen;
    uint8_t slot;
    const char *title;
} menu_item_t;

typedef struct {
    hvac_mode_t mode;
    const char *title;
} mode_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} system_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} system_time_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} installer_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} room_thermostat_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} io_config_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} network_setup_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} wifi_setup_menu_item_t;

typedef struct {
    ui_screen_t screen;
    const char *title;
} service_menu_item_t;

typedef struct {
    uint16_t ac_start_delay_s;
    uint16_t aux_relay_delay_s;
} delay_settings_t;

typedef enum {
    ZONE_DETAIL_CONNECTION = 0,
    ZONE_DETAIL_TEMP_SOURCE,
    ZONE_DETAIL_CHRONOGRAM
} zone_detail_item_id_t;

typedef struct {
    const char *title;
    zone_detail_item_id_t id;
} zone_detail_menu_item_t;

typedef enum {
    ZONE_TEMP_SOURCE_THERMOSTAT = 0,
    ZONE_TEMP_SOURCE_CHRONOGRAM
} zone_temp_source_t;

typedef struct {
    uint8_t start_hour;
    uint8_t start_minute;
    uint8_t end_hour;
    uint8_t end_minute;
    uint8_t target_temp_c;
} zone_schedule_entry_t;

typedef struct {
    bool paired;
    zone_temp_source_t temp_source;
    uint8_t base_set_temp_c;
    zone_schedule_entry_t schedule[3];
    hvac_mode_t mode;
    bool output_on;
    bool link_up;
    uint8_t tx_id;
    uint8_t rssi_percent;
    float current_temp_c;
    float thermostat_target_temp_c;
    float target_temp_c;
} zone_state_t;

typedef struct {
    ui_screen_t current_screen;
    size_t selected_root_index;
    size_t selected_mode_index;
    size_t selected_system_index;
    size_t selected_system_time_index;
    size_t selected_installer_index;
    size_t selected_room_thermostat_index;
    size_t selected_io_config_index;
    size_t selected_network_setup_index;
    size_t selected_wifi_setup_index;
    size_t selected_service_index;
    size_t selected_delay_index;
    size_t selected_factory_reset_index;
    size_t selected_zone_index;
    size_t selected_zone_detail_index;
    size_t selected_zone_connection_index;
    size_t selected_zone_sync_confirm_index;
    size_t selected_zone_temp_source_index;
    size_t selected_chronogram_index;
    size_t dashboard_zone_scroll;
    int64_t zone_sync_wait_deadline_ms;
    bool root_menu_active;
    bool delay_edit_active;
    uint8_t selected_date_field_index;
    uint8_t selected_time_field_index;
} ui_state_t;

typedef struct {
    float current_temp_c;
    float target_temp_c;
    float auto_hysteresis_c;
    float temp_calibration_c;
    bool relay_on;
    bool rx_link;
    bool wifi_connected;
    bool ethernet_connected;
    bool mqtt_connected;
    hvac_mode_t mode;
    char last_uart_line[96];
} klim_state_t;

static klim_state_t s_state = {
    .current_temp_c = 0.0f,
    .target_temp_c = 24.0f,
    .auto_hysteresis_c = AUTO_HYSTERESIS_DEFAULT_C,
    .temp_calibration_c = TEMP_CALIBRATION_DEFAULT_C,
    .relay_on = false,
    .rx_link = false,
    .wifi_connected = false,
    .mqtt_connected = false,
    .mode = HVAC_MODE_COOL,
    .last_uart_line = {0},
};

static delay_settings_t s_delay_settings = {
    .ac_start_delay_s = AC_START_DELAY_DEFAULT_S,
    .aux_relay_delay_s = AUX_RELAY_DELAY_DEFAULT_S,
};

static const menu_item_t s_root_menu_items[] = {
    {UI_SCREEN_WORK_MODE, 1u, "MODE"},
    {UI_SCREEN_ZONES,     2u, "ZONES"},
    {UI_SCREEN_SYSTEM,    3u, "SYSTEM"},
    {UI_SCREEN_INSTALLER, 4u, "INSTALLER MENU"},
    {UI_SCREEN_SERVICE,   5u, "SERVICE MENU"},
};

static const mode_menu_item_t s_mode_menu_items[] = {
    {HVAC_MODE_COOL,     "COOL"},
    {HVAC_MODE_HEAT,     "HEAT"},
    {HVAC_MODE_AUTO,     "AUTO"},
    {HVAC_MODE_FAN_ONLY, "FAN"},
};

static const system_menu_item_t s_system_menu_items[] = {
    {UI_SCREEN_SYSTEM_TIME,     "DATE/TIME"},
    {UI_SCREEN_SYSTEM_DISPLAY,  "DISPLAY SETUP"},
    {UI_SCREEN_SYSTEM_LANGUAGE, "SYSTEM LANGUAGE"},
};

static const system_time_menu_item_t s_system_time_menu_items[] = {
    {UI_SCREEN_SYSTEM_DATE_SETUP, "DATE SETUP"},
    {UI_SCREEN_SYSTEM_TIME_SETUP, "TIME SETUP"},
};

static const installer_menu_item_t s_installer_menu_items[] = {
    {UI_SCREEN_INSTALLER_ROOM_THERMOSTAT, "ROOM THERMOSTAT"},
    {UI_SCREEN_INSTALLER_IO_CONFIG,       "I/O CONFIG"},
    {UI_SCREEN_INSTALLER_NETWORK_SETUP,   "NETWORK SETUP"},
    {UI_SCREEN_INSTALLER_WIFI_SETUP,      "WIFI SETUP"},
    {UI_SCREEN_INSTALLER_OUTDOOR_SENSOR,  "OUTDOOR SENSOR"},
    {UI_SCREEN_INSTALLER_DELAY,           "DELAY"},
    {UI_SCREEN_INSTALLER_FACTORY_RESET,   "FACTORY RESET"},
};

static const room_thermostat_menu_item_t s_room_thermostat_menu_items[] = {
    {UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS,  "HYSTERESIS"},
    {UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION, "CALIBRATION"},
};

static const io_config_menu_item_t s_io_config_menu_items[] = {
    {UI_SCREEN_INSTALLER_IO_ZONE_RELAYS,        "ZONE RELAYS"},
    {UI_SCREEN_INSTALLER_IO_DRY_CONTACT_RELAY,  "DRY CONTACT RELAY"},
    {UI_SCREEN_INSTALLER_IO_ZONE_AUX_SENSORS,   "ZONE AUX SENSORS"},
};

static const network_setup_menu_item_t s_network_setup_menu_items[] = {
    {UI_SCREEN_INSTALLER_NETWORK_DHCP,         "DHCP"},
    {UI_SCREEN_INSTALLER_NETWORK_IP_ADDRESS,   "IP ADDRESS"},
    {UI_SCREEN_INSTALLER_NETWORK_SUBNET_MASK,  "SUBNET MASK"},
    {UI_SCREEN_INSTALLER_NETWORK_GATEWAY,      "GATEWAY"},
    {UI_SCREEN_INSTALLER_NETWORK_DNS,          "DNS"},
    {UI_SCREEN_INSTALLER_NETWORK_HOSTNAME,     "HOSTNAME"},
    {UI_SCREEN_INSTALLER_NETWORK_LINK_STATUS,  "LINK STATUS"},
    {UI_SCREEN_INSTALLER_NETWORK_MAC_ADDRESS,  "MAC ADDRESS"},
};

static const wifi_setup_menu_item_t s_wifi_setup_menu_items[] = {
    {UI_SCREEN_INSTALLER_WIFI_ENABLE,      "ENABLE"},
    {UI_SCREEN_INSTALLER_WIFI_SSID,        "SSID"},
    {UI_SCREEN_INSTALLER_WIFI_PASSWORD,    "PASSWORD"},
    {UI_SCREEN_INSTALLER_WIFI_DHCP,        "DHCP"},
    {UI_SCREEN_INSTALLER_WIFI_IP_ADDRESS,  "IP ADDRESS"},
    {UI_SCREEN_INSTALLER_WIFI_GATEWAY,     "GATEWAY"},
    {UI_SCREEN_INSTALLER_WIFI_STATUS,      "STATUS"},
    {UI_SCREEN_INSTALLER_WIFI_SIGNAL,      "SIGNAL"},
};

static const service_menu_item_t s_service_menu_items[] = {
    {UI_SCREEN_INSTALLER_MANUAL_CONTROL, "MANUAL CONTROL"},
};

static const char *s_delay_item_titles[] = {
    "A/C START DELAY",
    "AUX RELAY DELAY",
};

static const char *s_factory_reset_items[] = {
    "YES",
    "NO",
};

static const zone_detail_menu_item_t s_zone_detail_menu_items[] = {
    {"CONNECTION",  ZONE_DETAIL_CONNECTION},
    {"TEMP SOURCE", ZONE_DETAIL_TEMP_SOURCE},
    {"CHRONOGRAM",  ZONE_DETAIL_CHRONOGRAM},
};

static const char *s_zone_connection_menu_items[] = {
    "SYNCHRONIZE",
    "UNPAIR",
};

static const char *s_zone_temp_source_menu_items[] = {
    "THERMOSTAT",
    "CHRONOGRAM",
};

static zone_state_t s_zones[ZONE_MAX_COUNT];

static ui_state_t s_ui = {
    .current_screen = UI_SCREEN_BOOT,
    .selected_root_index = 0u,
    .selected_mode_index = 0u,
    .selected_system_index = 0u,
    .selected_system_time_index = 0u,
    .selected_installer_index = 0u,
    .selected_room_thermostat_index = 0u,
    .selected_io_config_index = 0u,
    .selected_network_setup_index = 0u,
    .selected_wifi_setup_index = 0u,
    .selected_service_index = 0u,
    .selected_delay_index = 0u,
    .selected_factory_reset_index = 1u,
    .selected_zone_index = 0u,
    .selected_zone_detail_index = 0u,
    .selected_zone_connection_index = 0u,
    .selected_zone_sync_confirm_index = 1u,
    .selected_zone_temp_source_index = 0u,
    .selected_chronogram_index = 0u,
    .dashboard_zone_scroll = 0u,
    .zone_sync_wait_deadline_ms = 0,
    .root_menu_active = false,
    .delay_edit_active = false,
    .selected_date_field_index = 0u,
    .selected_time_field_index = 0u,
};
static volatile bool s_home_live_update_pending = false;

static klim_state_t state_snapshot(void);
static void draw_current_screen(void);
static void draw_placeholder_screen(const char *title);
static const char *mode_to_string(hvac_mode_t mode);
static const char *mode_to_ui_label(hvac_mode_t mode);
static void handle_mode_command(const char *value);
static void zones_init_defaults(void);
static void zones_sync_live_state_from_controller(void);
static void state_lock(void);
static void state_unlock(void);
static void uart_send_command(const klim_state_t *state);
static void uart_send_mode_override(hvac_mode_t mode);
static void uart_send_target_override(float target_c);
static void uart_write_text_slow(const char *line, TickType_t inter_byte_delay_ticks);
static void mqtt_publish_state(void);
static void clock_init_factory_default(void);
static void clock_reset_factory_default(void);
static void clock_get_display_strings(char *date_buf, size_t date_buf_len, char *time_buf, size_t time_buf_len);
static void clock_get_tm(struct tm *tm_value);
static void clock_load_edit_tm(void);
static void clock_adjust_date_field(int step);
static void clock_adjust_time_field(int step);
static int64_t ui_now_ms(void);
static float clamp_target_temp(float value);
static float clamp_hysteresis_value(float value);
static float clamp_calibration_value(float value);
static bool zone_refresh_effective_target(zone_state_t *zone);
static void uart_send_zone_sync_request(uint8_t zone);
static void uart_send_zone_unpair_request(uint8_t zone);
static void uart_tx_lock(void);
static void uart_tx_unlock(void);
static void ds1302_init_pins(void);

static inline uint16_t lcd_swap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static void lcd_spi_pre_transfer_cb(spi_transaction_t *t)
{
    int dc = (int)(intptr_t)t->user;
    gpio_set_level(PIN_TFT_DC, dc);
}

static esp_err_t lcd_write_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0,
    };
    return spi_device_polling_transmit(s_lcd, &t);
}

static esp_err_t lcd_write_data(const void *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .user = (void *)1,
    };
    return spi_device_polling_transmit(s_lcd, &t);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)(x0 & 0xFF);
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)(x1 & 0xFF);
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_CASET));
    ESP_ERROR_CHECK(lcd_write_data(data, sizeof(data)));

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)(y0 & 0xFF);
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)(y1 & 0xFF);
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_RASET));
    ESP_ERROR_CHECK(lcd_write_data(data, sizeof(data)));

    ESP_ERROR_CHECK(lcd_write_cmd(CMD_RAMWR));
}

static void lcd_reset(void)
{
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void lcd_init_panel(void)
{
    static const uint8_t frmctr1[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr2[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t invctr[]  = {0x07};
    static const uint8_t pwctr1[]  = {0xA2, 0x02, 0x84};
    static const uint8_t pwctr2[]  = {0xC5};
    static const uint8_t pwctr3[]  = {0x0A, 0x00};
    static const uint8_t pwctr4[]  = {0x8A, 0x2A};
    static const uint8_t pwctr5[]  = {0x8A, 0xEE};
    static const uint8_t vmctr1[]  = {0x0E};
    static const uint8_t madctl[]  = {0xC0};
    static const uint8_t colmod[]  = {0x05};
    static const uint8_t gmctrp1[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
    static const uint8_t gmctrn1[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};

    lcd_reset();

    ESP_ERROR_CHECK(lcd_write_cmd(CMD_SWRESET));
    vTaskDelay(pdMS_TO_TICKS(150));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_SLPOUT));
    vTaskDelay(pdMS_TO_TICKS(150));

    ESP_ERROR_CHECK(lcd_write_cmd(CMD_FRMCTR1));
    ESP_ERROR_CHECK(lcd_write_data(frmctr1, sizeof(frmctr1)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_FRMCTR2));
    ESP_ERROR_CHECK(lcd_write_data(frmctr2, sizeof(frmctr2)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_FRMCTR3));
    ESP_ERROR_CHECK(lcd_write_data(frmctr3, sizeof(frmctr3)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_INVCTR));
    ESP_ERROR_CHECK(lcd_write_data(invctr, sizeof(invctr)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_PWCTR1));
    ESP_ERROR_CHECK(lcd_write_data(pwctr1, sizeof(pwctr1)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_PWCTR2));
    ESP_ERROR_CHECK(lcd_write_data(pwctr2, sizeof(pwctr2)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_PWCTR3));
    ESP_ERROR_CHECK(lcd_write_data(pwctr3, sizeof(pwctr3)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_PWCTR4));
    ESP_ERROR_CHECK(lcd_write_data(pwctr4, sizeof(pwctr4)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_PWCTR5));
    ESP_ERROR_CHECK(lcd_write_data(pwctr5, sizeof(pwctr5)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_VMCTR1));
    ESP_ERROR_CHECK(lcd_write_data(vmctr1, sizeof(vmctr1)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_INVOFF));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_MADCTL));
    ESP_ERROR_CHECK(lcd_write_data(madctl, sizeof(madctl)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_COLMOD));
    ESP_ERROR_CHECK(lcd_write_data(colmod, sizeof(colmod)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_GMCTRP1));
    ESP_ERROR_CHECK(lcd_write_data(gmctrp1, sizeof(gmctrp1)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_GMCTRN1));
    ESP_ERROR_CHECK(lcd_write_data(gmctrn1, sizeof(gmctrn1)));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_NORON));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(lcd_write_cmd(CMD_DISPON));
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > LCD_WIDTH) {
        w = LCD_WIDTH - x;
    }
    if (y + h > LCD_HEIGHT) {
        h = LCD_HEIGHT - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    uint16_t panel_color = lcd_swap16(color);
    for (int row = 0; row < h; ++row) {
        uint16_t *dst = &s_lcd_framebuffer[(size_t)(y + row) * LCD_WIDTH + (size_t)x];
        for (int col = 0; col < w; ++col) {
            dst[col] = panel_color;
        }
    }
}

static void lcd_fill_screen(uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

static void lcd_present(void)
{
    lcd_set_window(0u, 0u, (uint16_t)(LCD_WIDTH - 1), (uint16_t)(LCD_HEIGHT - 1));
    for (int row = 0; row < LCD_HEIGHT; ++row) {
        ESP_ERROR_CHECK(lcd_write_data(&s_lcd_framebuffer[row * LCD_WIDTH], (size_t)LCD_WIDTH * sizeof(uint16_t)));
    }
}

static void lcd_draw_thick_point(int x, int y, int size, uint16_t color)
{
    lcd_fill_rect(x - size / 2, y - size / 2, size, size, color);
}

static void lcd_draw_thick_line(int x0, int y0, int x1, int y1, int size, uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        lcd_draw_thick_point(x0, y0, size, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void lcd_fill_rect_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    int rx = LCD_WIDTH - (y + h);
    int ry = x;
    lcd_fill_rect(rx, ry, h, w, color);
}

static void lcd_draw_thick_point_rot90cw(int x, int y, int size, uint16_t color)
{
    lcd_fill_rect_rot90cw(x - size / 2, y - size / 2, size, size, color);
}

static void lcd_draw_thick_line_rot90cw(int x0, int y0, int x1, int y1, int size, uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        lcd_draw_thick_point_rot90cw(x0, y0, size, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void lcd_draw_rect_rot90cw(int x, int y, int w, int h, int thickness, uint16_t color)
{
    if ((w <= 0) || (h <= 0) || (thickness <= 0)) {
        return;
    }

    lcd_fill_rect_rot90cw(x, y, w, thickness, color);
    lcd_fill_rect_rot90cw(x, y + h - thickness, w, thickness, color);
    lcd_fill_rect_rot90cw(x, y, thickness, h, color);
    lcd_fill_rect_rot90cw(x + w - thickness, y, thickness, h, color);
}

static void lcd_fill_triangle_rot90cw(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color)
{
    if (y1 < y0) {
        int tx = x0, ty = y0;
        x0 = x1; y0 = y1;
        x1 = tx; y1 = ty;
    }
    if (y2 < y0) {
        int tx = x0, ty = y0;
        x0 = x2; y0 = y2;
        x2 = tx; y2 = ty;
    }
    if (y2 < y1) {
        int tx = x1, ty = y1;
        x1 = x2; y1 = y2;
        x2 = tx; y2 = ty;
    }

    if (y0 == y2) {
        int min_x = x0;
        int max_x = x0;
        if (x1 < min_x) min_x = x1;
        if (x2 < min_x) min_x = x2;
        if (x1 > max_x) max_x = x1;
        if (x2 > max_x) max_x = x2;
        lcd_fill_rect_rot90cw(min_x, y0, max_x - min_x + 1, 1, color);
        return;
    }

    for (int y = y0; y <= y2; ++y) {
        int a = x0;
        int b = x0;

        if (y2 != y0) {
            a = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        }

        if (y < y1) {
            if (y1 != y0) {
                b = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            }
        } else {
            if (y2 != y1) {
                b = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            } else {
                b = x2;
            }
        }

        if (a > b) {
            int t = a;
            a = b;
            b = t;
        }
        lcd_fill_rect_rot90cw(a, y, b - a + 1, 1, color);
    }
}

static void draw_snowflake_rot90cw(int cx, int cy, int r, int thickness, uint16_t color)
{
    lcd_draw_thick_line_rot90cw(cx - r, cy, cx + r, cy, thickness, color);
    lcd_draw_thick_line_rot90cw(cx, cy - r, cx, cy + r, thickness, color);
    lcd_draw_thick_line_rot90cw(cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1, thickness, color);
    lcd_draw_thick_line_rot90cw(cx - r + 1, cy + r - 1, cx + r - 1, cy - r + 1, thickness, color);
}

static void draw_logo_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    int mountain_base = y + h - 6;
    int left_peak_x = x + w / 4;
    int center_peak_x = x + w / 2;
    int right_peak_x = x + (w * 3) / 4;
    int left_peak_y = y + h / 2 + 2;
    int center_peak_y = y + 6;
    int right_peak_y = y + h / 2 + 8;

    lcd_fill_triangle_rot90cw(x + 6, mountain_base, left_peak_x, left_peak_y, center_peak_x - 8, mountain_base, color);
    lcd_fill_triangle_rot90cw(left_peak_x - 4, mountain_base, center_peak_x, center_peak_y, right_peak_x + 2, mountain_base, color);
    lcd_fill_triangle_rot90cw(center_peak_x + 6, mountain_base, right_peak_x, right_peak_y, x + w - 8, mountain_base, color);

    lcd_fill_triangle_rot90cw(center_peak_x - 6, y + h / 2, center_peak_x + 6, y + h / 2 + 8, center_peak_x + 2, y + h / 3, 0xFFFF);
    draw_snowflake_rot90cw(x + w / 4 - 6, y + h / 4, 8, 2, color);
}

static const uint8_t GLYPH_A[12] = {
    0x18, 0x18, 0x24, 0x24,
    0x42, 0x42, 0x7E, 0x7E,
    0x42, 0x42, 0x42, 0x00
};

static const uint8_t GLYPH_J[12] = {
    0x7E, 0x7E, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18,
    0x58, 0x58, 0x30, 0x00
};

static const uint8_t GLYPH_X[12] = {
    0x42, 0x42, 0x24, 0x24,
    0x18, 0x18, 0x18, 0x18,
    0x24, 0x24, 0x42, 0x00
};

static const uint8_t GLYPH_K[12] = {
    0x42, 0x44, 0x48, 0x50,
    0x60, 0x50, 0x48, 0x44,
    0x42, 0x42, 0x42, 0x00
};

static const uint8_t GLYPH_L[12] = {
    0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x40, 0x40,
    0x40, 0x40, 0x7E, 0x00
};

static const uint8_t GLYPH_I[12] = {
    0x7E, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x7E, 0x00
};

static const uint8_t GLYPH_M[12] = {
    0x42, 0x66, 0x5A, 0x5A,
    0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x00
};

static const uint8_t FONT5_SPACE[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FONT5_DOT[7]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
static const uint8_t FONT5_COLON[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
static const uint8_t FONT5_DASH[7]  = {0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FONT5_PERCENT[7] = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
static const uint8_t FONT5_SLASH[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};

static const uint8_t FONT5_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
static const uint8_t FONT5_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t FONT5_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
static const uint8_t FONT5_3[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
static const uint8_t FONT5_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
static const uint8_t FONT5_5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
static const uint8_t FONT5_6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
static const uint8_t FONT5_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
static const uint8_t FONT5_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
static const uint8_t FONT5_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C};

static const uint8_t FONT5_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t FONT5_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
static const uint8_t FONT5_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
static const uint8_t FONT5_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
static const uint8_t FONT5_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
static const uint8_t FONT5_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t FONT5_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
static const uint8_t FONT5_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t FONT5_I[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
static const uint8_t FONT5_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
static const uint8_t FONT5_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
static const uint8_t FONT5_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
static const uint8_t FONT5_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
static const uint8_t FONT5_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t FONT5_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t FONT5_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
static const uint8_t FONT5_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
static const uint8_t FONT5_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
static const uint8_t FONT5_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t FONT5_V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
static const uint8_t FONT5_Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
static const uint8_t FONT5_Z[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

static const uint8_t *font5x7_get(char ch)
{
    switch (ch) {
    case '0': return FONT5_0;
    case '1': return FONT5_1;
    case '2': return FONT5_2;
    case '3': return FONT5_3;
    case '4': return FONT5_4;
    case '5': return FONT5_5;
    case '6': return FONT5_6;
    case '7': return FONT5_7;
    case '8': return FONT5_8;
    case '9': return FONT5_9;
    case 'A': return FONT5_A;
    case 'B': return FONT5_B;
    case 'C': return FONT5_C;
    case 'D': return FONT5_D;
    case 'E': return FONT5_E;
    case 'F': return FONT5_F;
    case 'G': return FONT5_G;
    case 'H': return FONT5_H;
    case 'I': return FONT5_I;
    case 'K': return FONT5_K;
    case 'L': return FONT5_L;
    case 'M': return FONT5_M;
    case 'N': return FONT5_N;
    case 'O': return FONT5_O;
    case 'P': return FONT5_P;
    case 'R': return FONT5_R;
    case 'S': return FONT5_S;
    case 'T': return FONT5_T;
    case 'U': return FONT5_U;
    case 'V': return FONT5_V;
    case 'Y': return FONT5_Y;
    case 'Z': return FONT5_Z;
    case '.': return FONT5_DOT;
    case ':': return FONT5_COLON;
    case '-': return FONT5_DASH;
    case '%': return FONT5_PERCENT;
    case '/': return FONT5_SLASH;
    case ' ':
    default:
        return FONT5_SPACE;
    }
}

static void draw_glyph_rot90cw(int x, int y, int w, int h, const uint8_t glyph[12], uint16_t color)
{
    const int glyph_w = 8;
    const int glyph_h = 12;
    int px_w = w / glyph_w;
    int px_h = h / glyph_h;
    int off_x;
    int off_y;

    if (px_w < 1) {
        px_w = 1;
    }
    if (px_h < 1) {
        px_h = 1;
    }

    off_x = x + (w - px_w * glyph_w) / 2;
    off_y = y + (h - px_h * glyph_h) / 2;

    for (int row = 0; row < glyph_h; ++row) {
        for (int col = 0; col < glyph_w; ++col) {
            if ((glyph[row] & (1u << (glyph_w - 1 - col))) != 0u) {
                lcd_fill_rect_rot90cw(off_x + col * px_w, off_y + row * px_h, px_w, px_h, color);
            }
        }
    }
}

static void draw_letter_a_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    draw_glyph_rot90cw(x, y, w, h, GLYPH_A, color);
}

static void draw_letter_j_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    draw_glyph_rot90cw(x, y, w, h, GLYPH_J, color);
}

static void draw_letter_x_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    draw_glyph_rot90cw(x, y, w, h, GLYPH_X, color);
}

static size_t ui_find_root_index(ui_screen_t screen)
{
    for (size_t i = 0; i < (sizeof(s_root_menu_items) / sizeof(s_root_menu_items[0])); ++i) {
        if (s_root_menu_items[i].screen == screen) {
            return i;
        }
    }
    return 0u;
}

static const menu_item_t *ui_get_selected_root_item(void)
{
    return &s_root_menu_items[s_ui.selected_root_index];
}

static size_t ui_root_item_count(void)
{
    return sizeof(s_root_menu_items) / sizeof(s_root_menu_items[0]);
}

static size_t ui_mode_item_count(void)
{
    return sizeof(s_mode_menu_items) / sizeof(s_mode_menu_items[0]);
}

static size_t ui_system_item_count(void)
{
    return sizeof(s_system_menu_items) / sizeof(s_system_menu_items[0]);
}

static size_t ui_system_time_item_count(void)
{
    return sizeof(s_system_time_menu_items) / sizeof(s_system_time_menu_items[0]);
}

static size_t ui_installer_item_count(void)
{
    return sizeof(s_installer_menu_items) / sizeof(s_installer_menu_items[0]);
}

static size_t ui_room_thermostat_item_count(void)
{
    return sizeof(s_room_thermostat_menu_items) / sizeof(s_room_thermostat_menu_items[0]);
}

static size_t ui_io_config_item_count(void)
{
    return sizeof(s_io_config_menu_items) / sizeof(s_io_config_menu_items[0]);
}

static size_t ui_network_setup_item_count(void)
{
    return sizeof(s_network_setup_menu_items) / sizeof(s_network_setup_menu_items[0]);
}

static size_t ui_wifi_setup_item_count(void)
{
    return sizeof(s_wifi_setup_menu_items) / sizeof(s_wifi_setup_menu_items[0]);
}

static size_t ui_service_item_count(void)
{
    return sizeof(s_service_menu_items) / sizeof(s_service_menu_items[0]);
}

static size_t ui_delay_item_count(void)
{
    return sizeof(s_delay_item_titles) / sizeof(s_delay_item_titles[0]);
}

static size_t ui_factory_reset_item_count(void)
{
    return sizeof(s_factory_reset_items) / sizeof(s_factory_reset_items[0]);
}

static size_t ui_zone_item_count(void)
{
    return (ZONE_ACTIVE_COUNT <= ZONE_MAX_COUNT) ? ZONE_ACTIVE_COUNT : ZONE_MAX_COUNT;
}

static size_t ui_dashboard_zone_scroll_max(void)
{
    const size_t zone_count = ui_zone_item_count();

    return (zone_count > DASHBOARD_ZONE_SLOTS) ? (zone_count - DASHBOARD_ZONE_SLOTS) : 0u;
}

static void ui_clamp_dashboard_zone_scroll(void)
{
    const size_t max_scroll = ui_dashboard_zone_scroll_max();

    if (s_ui.dashboard_zone_scroll > max_scroll) {
        s_ui.dashboard_zone_scroll = max_scroll;
    }
}

static size_t ui_dashboard_visible_zone_count(void)
{
    size_t zone_count = ui_zone_item_count();

    ui_clamp_dashboard_zone_scroll();
    if (zone_count <= s_ui.dashboard_zone_scroll) {
        return 0u;
    }

    zone_count -= s_ui.dashboard_zone_scroll;
    return (zone_count > DASHBOARD_ZONE_SLOTS) ? DASHBOARD_ZONE_SLOTS : zone_count;
}

static int ui_find_first_paired_zone_index(void)
{
    const size_t count = ui_zone_item_count();

    for (size_t i = 0; i < count; ++i) {
        if (s_zones[i].paired) {
            return (int)i;
        }
    }
    return -1;
}

static bool ui_sync_dashboard_selected_zone(void)
{
    const size_t count = ui_zone_item_count();
    const int first_paired = ui_find_first_paired_zone_index();

    if ((count == 0u) || (first_paired < 0)) {
        s_ui.dashboard_zone_scroll = 0u;
        return false;
    }

    if ((s_ui.selected_zone_index >= count) || !s_zones[s_ui.selected_zone_index].paired) {
        s_ui.selected_zone_index = (size_t)first_paired;
    }

    if (s_ui.selected_zone_index < s_ui.dashboard_zone_scroll) {
        s_ui.dashboard_zone_scroll = s_ui.selected_zone_index;
    } else if (s_ui.selected_zone_index >= (s_ui.dashboard_zone_scroll + DASHBOARD_ZONE_SLOTS)) {
        s_ui.dashboard_zone_scroll = s_ui.selected_zone_index - (DASHBOARD_ZONE_SLOTS - 1u);
    }

    ui_clamp_dashboard_zone_scroll();
    return true;
}

static size_t ui_zone_detail_item_count(void)
{
    return sizeof(s_zone_detail_menu_items) / sizeof(s_zone_detail_menu_items[0]);
}

static size_t ui_zone_connection_item_count(void)
{
    return sizeof(s_zone_connection_menu_items) / sizeof(s_zone_connection_menu_items[0]);
}

static size_t ui_zone_temp_source_item_count(void)
{
    return sizeof(s_zone_temp_source_menu_items) / sizeof(s_zone_temp_source_menu_items[0]);
}

static size_t ui_mode_index_from_hvac_mode(hvac_mode_t mode)
{
    for (size_t i = 0; i < ui_mode_item_count(); ++i) {
        if (s_mode_menu_items[i].mode == mode) {
            return i;
        }
    }
    return 0u;
}

static hvac_mode_t ui_hvac_mode_from_mode_index(size_t index)
{
    if (index >= ui_mode_item_count()) {
        return s_mode_menu_items[0].mode;
    }
    return s_mode_menu_items[index].mode;
}

static const zone_detail_menu_item_t *ui_get_selected_zone_detail_item(void)
{
    return &s_zone_detail_menu_items[s_ui.selected_zone_detail_index];
}

static zone_state_t *ui_get_selected_zone_state(void)
{
    return &s_zones[s_ui.selected_zone_index];
}

static const system_menu_item_t *ui_get_selected_system_item(void)
{
    return &s_system_menu_items[s_ui.selected_system_index];
}

static const installer_menu_item_t *ui_get_selected_installer_item(void)
{
    return &s_installer_menu_items[s_ui.selected_installer_index];
}

static const room_thermostat_menu_item_t *ui_get_selected_room_thermostat_item(void)
{
    return &s_room_thermostat_menu_items[s_ui.selected_room_thermostat_index];
}

static const io_config_menu_item_t *ui_get_selected_io_config_item(void)
{
    return &s_io_config_menu_items[s_ui.selected_io_config_index];
}

static const network_setup_menu_item_t *ui_get_selected_network_setup_item(void)
{
    return &s_network_setup_menu_items[s_ui.selected_network_setup_index];
}

static const wifi_setup_menu_item_t *ui_get_selected_wifi_setup_item(void)
{
    return &s_wifi_setup_menu_items[s_ui.selected_wifi_setup_index];
}

static const service_menu_item_t *ui_get_selected_service_item(void)
{
    return &s_service_menu_items[s_ui.selected_service_index];
}

static uint8_t ui_get_selected_zone_number(void)
{
    return (uint8_t)(s_ui.selected_zone_index + 1u);
}

static void ui_move_dashboard_zone_scroll(int step)
{
    const size_t count = ui_zone_item_count();

    if ((count == 0u) || !ui_sync_dashboard_selected_zone()) {
        return;
    }

    if (step != 0) {
        size_t next = s_ui.selected_zone_index;

        for (size_t i = 0; i < count; ++i) {
            if (step > 0) {
                next = (next + 1u) % count;
            } else {
                next = (next == 0u) ? (count - 1u) : (next - 1u);
            }

            if (s_zones[next].paired) {
                s_ui.selected_zone_index = next;
                break;
            }
        }
    }

    ui_sync_dashboard_selected_zone();
}

static void ui_set_screen(ui_screen_t screen)
{
    s_ui.current_screen = screen;
    s_ui.selected_root_index = ui_find_root_index(screen);
    s_ui.root_menu_active = false;
    s_ui.delay_edit_active = false;
    if (screen == UI_SCREEN_WORK_MODE) {
        const klim_state_t state = state_snapshot();
        s_ui.selected_mode_index = ui_mode_index_from_hvac_mode(state.mode);
    }
}

static void ui_show_home_preserve_selection(void)
{
    s_ui.current_screen = UI_SCREEN_HOME;
    s_ui.root_menu_active = false;
}

static void ui_activate_root_menu(void)
{
    s_ui.current_screen = UI_SCREEN_HOME;
    s_ui.root_menu_active = true;
}

static void ui_deactivate_root_menu(void)
{
    s_ui.root_menu_active = false;
}

static void ui_move_root_selection(int step)
{
    const size_t count = ui_root_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_root_index = (s_ui.selected_root_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_root_index = (s_ui.selected_root_index == 0u) ? (count - 1u) : (s_ui.selected_root_index - 1u);
    }
}

static void ui_move_mode_selection(int step)
{
    const size_t count = ui_mode_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_mode_index = (s_ui.selected_mode_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_mode_index = (s_ui.selected_mode_index == 0u) ? (count - 1u) : (s_ui.selected_mode_index - 1u);
    }
}

static void ui_move_system_selection(int step)
{
    const size_t count = ui_system_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_system_index = (s_ui.selected_system_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_system_index = (s_ui.selected_system_index == 0u) ? (count - 1u) : (s_ui.selected_system_index - 1u);
    }
}

static void ui_move_system_time_selection(int step)
{
    const size_t count = ui_system_time_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_system_time_index = (s_ui.selected_system_time_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_system_time_index = (s_ui.selected_system_time_index == 0u) ? (count - 1u) : (s_ui.selected_system_time_index - 1u);
    }
}

static void ui_move_installer_selection(int step)
{
    const size_t count = ui_installer_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_installer_index = (s_ui.selected_installer_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_installer_index = (s_ui.selected_installer_index == 0u) ? (count - 1u) : (s_ui.selected_installer_index - 1u);
    }
}

static void ui_move_room_thermostat_selection(int step)
{
    const size_t count = ui_room_thermostat_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_room_thermostat_index = (s_ui.selected_room_thermostat_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_room_thermostat_index = (s_ui.selected_room_thermostat_index == 0u) ? (count - 1u) : (s_ui.selected_room_thermostat_index - 1u);
    }
}

static void ui_move_io_config_selection(int step)
{
    const size_t count = ui_io_config_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_io_config_index = (s_ui.selected_io_config_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_io_config_index = (s_ui.selected_io_config_index == 0u) ? (count - 1u) : (s_ui.selected_io_config_index - 1u);
    }
}

static void ui_move_network_setup_selection(int step)
{
    const size_t count = ui_network_setup_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_network_setup_index = (s_ui.selected_network_setup_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_network_setup_index = (s_ui.selected_network_setup_index == 0u) ? (count - 1u) : (s_ui.selected_network_setup_index - 1u);
    }
}

static void ui_move_wifi_setup_selection(int step)
{
    const size_t count = ui_wifi_setup_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_wifi_setup_index = (s_ui.selected_wifi_setup_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_wifi_setup_index = (s_ui.selected_wifi_setup_index == 0u) ? (count - 1u) : (s_ui.selected_wifi_setup_index - 1u);
    }
}

static void ui_move_service_selection(int step)
{
    const size_t count = ui_service_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_service_index = (s_ui.selected_service_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_service_index = (s_ui.selected_service_index == 0u) ? (count - 1u) : (s_ui.selected_service_index - 1u);
    }
}

static void ui_move_delay_selection(int step)
{
    const size_t count = ui_delay_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_delay_index = (s_ui.selected_delay_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_delay_index = (s_ui.selected_delay_index == 0u) ? (count - 1u) : (s_ui.selected_delay_index - 1u);
    }
}

static uint16_t clamp_u16(uint16_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void ui_adjust_delay_value(int step)
{
    if (s_ui.selected_delay_index == 0u) {
        int value = (int)s_delay_settings.ac_start_delay_s + step;
        s_delay_settings.ac_start_delay_s = clamp_u16((uint16_t)((value < 0) ? 0 : value),
                                                      AC_START_DELAY_MIN_S,
                                                      AC_START_DELAY_MAX_S);
    } else {
        int value = (int)s_delay_settings.aux_relay_delay_s + step;
        s_delay_settings.aux_relay_delay_s = clamp_u16((uint16_t)((value < 0) ? 0 : value),
                                                       AUX_RELAY_DELAY_MIN_S,
                                                       AUX_RELAY_DELAY_MAX_S);
    }
}

static void ui_adjust_room_thermostat_value(int step)
{
    state_lock();

    if (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS) {
        s_state.auto_hysteresis_c = clamp_hysteresis_value(
            s_state.auto_hysteresis_c + ((float)step * HYSTERESIS_STEP_C));
    } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION) {
        s_state.temp_calibration_c = clamp_calibration_value(
            s_state.temp_calibration_c + ((float)step * CALIBRATION_STEP_C));
    }

    state_unlock();
}

static void ui_move_factory_reset_selection(int step)
{
    const size_t count = ui_factory_reset_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_factory_reset_index = (s_ui.selected_factory_reset_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_factory_reset_index = (s_ui.selected_factory_reset_index == 0u) ? (count - 1u) : (s_ui.selected_factory_reset_index - 1u);
    }
}

static void apply_factory_reset(void)
{
    zones_init_defaults();
    clock_reset_factory_default();

    s_delay_settings.ac_start_delay_s = AC_START_DELAY_DEFAULT_S;
    s_delay_settings.aux_relay_delay_s = AUX_RELAY_DELAY_DEFAULT_S;

    state_lock();
    s_state.target_temp_c = TARGET_TEMP_DEFAULT_C;
    s_state.auto_hysteresis_c = AUTO_HYSTERESIS_DEFAULT_C;
    s_state.temp_calibration_c = TEMP_CALIBRATION_DEFAULT_C;
    s_state.mode = HVAC_MODE_COOL;
    s_state.relay_on = false;
    memset(s_state.last_uart_line, 0, sizeof(s_state.last_uart_line));
    state_unlock();

    s_ui.selected_mode_index = ui_mode_index_from_hvac_mode(HVAC_MODE_COOL);
    s_ui.selected_delay_index = 0u;
    s_ui.delay_edit_active = false;
    s_ui.selected_zone_index = 0u;
    s_ui.selected_zone_detail_index = 0u;
    s_ui.selected_zone_connection_index = 0u;
    s_ui.selected_zone_temp_source_index = 0u;
    s_ui.selected_chronogram_index = 0u;
    s_ui.dashboard_zone_scroll = 0u;

    {
        const klim_state_t state = state_snapshot();
        uart_send_command(&state);
        mqtt_publish_state();
    }
}

static void ui_move_zone_selection(int step)
{
    const size_t count = ui_zone_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_zone_index = (s_ui.selected_zone_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_zone_index = (s_ui.selected_zone_index == 0u) ? (count - 1u) : (s_ui.selected_zone_index - 1u);
    }
}

static void ui_move_zone_detail_selection(int step)
{
    const size_t count = ui_zone_detail_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_zone_detail_index = (s_ui.selected_zone_detail_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_zone_detail_index = (s_ui.selected_zone_detail_index == 0u) ? (count - 1u) : (s_ui.selected_zone_detail_index - 1u);
    }
}

static void ui_move_zone_connection_selection(int step)
{
    const size_t count = ui_zone_connection_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_zone_connection_index = (s_ui.selected_zone_connection_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_zone_connection_index = (s_ui.selected_zone_connection_index == 0u) ? (count - 1u) : (s_ui.selected_zone_connection_index - 1u);
    }
}

static void ui_move_zone_sync_confirm_selection(int step)
{
    if (step > 0) {
        s_ui.selected_zone_sync_confirm_index = (s_ui.selected_zone_sync_confirm_index + 1u) % 2u;
    } else if (step < 0) {
        s_ui.selected_zone_sync_confirm_index = (s_ui.selected_zone_sync_confirm_index == 0u) ? 1u : 0u;
    }
}

static void ui_move_zone_temp_source_selection(int step)
{
    const size_t count = ui_zone_temp_source_item_count();

    if (count == 0u) {
        return;
    }

    if (step > 0) {
        s_ui.selected_zone_temp_source_index = (s_ui.selected_zone_temp_source_index + 1u) % count;
    } else if (step < 0) {
        s_ui.selected_zone_temp_source_index = (s_ui.selected_zone_temp_source_index == 0u) ? (count - 1u) : (s_ui.selected_zone_temp_source_index - 1u);
    }
}

static bool button_is_pressed(gpio_num_t pin)
{
    return gpio_get_level(pin) == 0;
}

static void buttons_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_UP) |
                        (1ULL << PIN_BTN_DOWN) |
                        (1ULL << PIN_BTN_MENU) |
                        (1ULL << PIN_BTN_EXIT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static int64_t ui_now_ms(void)
{
    return esp_timer_get_time() / 1000LL;
}

static void ui_handle_button_press(gpio_num_t pin)
{
    bool redraw = false;

    switch (pin) {
    case PIN_BTN_UP:
        if ((s_ui.current_screen == UI_SCREEN_HOME) && s_ui.root_menu_active) {
            ui_move_root_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_HOME) {
            ui_move_dashboard_zone_scroll(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_WORK_MODE) {
            ui_move_mode_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM) {
            ui_move_system_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME) {
            ui_move_system_time_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_DATE_SETUP) {
            clock_adjust_date_field(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME_SETUP) {
            clock_adjust_time_field(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER) {
            ui_move_installer_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT) {
            ui_move_room_thermostat_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_CONFIG) {
            ui_move_io_config_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_SETUP) {
            ui_move_network_setup_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SETUP) {
            ui_move_wifi_setup_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SERVICE) {
            ui_move_service_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_DELAY) {
            if (s_ui.delay_edit_active) {
                ui_adjust_delay_value(-1);
            } else {
                ui_move_delay_selection(-1);
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_FACTORY_RESET) {
            ui_move_factory_reset_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONES) {
            ui_move_zone_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_DETAIL) {
            ui_move_zone_detail_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_CONNECTION) {
            ui_move_zone_connection_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_CONFIRM) {
            ui_move_zone_sync_confirm_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_TEMP_SOURCE) {
            ui_move_zone_temp_source_selection(-1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_CHRONOGRAM) {
            s_ui.selected_chronogram_index = (s_ui.selected_chronogram_index == 0u) ? 2u : (s_ui.selected_chronogram_index - 1u);
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION)) {
            ui_adjust_room_thermostat_value(-1);
            redraw = true;
        }
        break;

    case PIN_BTN_DOWN:
        if ((s_ui.current_screen == UI_SCREEN_HOME) && s_ui.root_menu_active) {
            ui_move_root_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_HOME) {
            ui_move_dashboard_zone_scroll(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_WORK_MODE) {
            ui_move_mode_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM) {
            ui_move_system_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME) {
            ui_move_system_time_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_DATE_SETUP) {
            clock_adjust_date_field(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME_SETUP) {
            clock_adjust_time_field(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER) {
            ui_move_installer_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT) {
            ui_move_room_thermostat_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_CONFIG) {
            ui_move_io_config_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_SETUP) {
            ui_move_network_setup_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SETUP) {
            ui_move_wifi_setup_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SERVICE) {
            ui_move_service_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_DELAY) {
            if (s_ui.delay_edit_active) {
                ui_adjust_delay_value(+1);
            } else {
                ui_move_delay_selection(+1);
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_FACTORY_RESET) {
            ui_move_factory_reset_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONES) {
            ui_move_zone_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_DETAIL) {
            ui_move_zone_detail_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_CONNECTION) {
            ui_move_zone_connection_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_CONFIRM) {
            ui_move_zone_sync_confirm_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_TEMP_SOURCE) {
            ui_move_zone_temp_source_selection(+1);
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_CHRONOGRAM) {
            s_ui.selected_chronogram_index = (s_ui.selected_chronogram_index + 1u) % 3u;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION)) {
            ui_adjust_room_thermostat_value(+1);
            redraw = true;
        }
        break;

    case PIN_BTN_MENU:
        if (s_ui.current_screen == UI_SCREEN_HOME) {
            if (!s_ui.root_menu_active) {
                ui_activate_root_menu();
            } else {
                ui_set_screen(ui_get_selected_root_item()->screen);
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_WORK_MODE) {
            handle_mode_command(mode_to_string(ui_hvac_mode_from_mode_index(s_ui.selected_mode_index)));
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM) {
            s_ui.current_screen = ui_get_selected_system_item()->screen;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME) {
            clock_load_edit_tm();
            if (s_ui.selected_system_time_index == 0u) {
                s_ui.selected_date_field_index = 0u;
                s_ui.current_screen = UI_SCREEN_SYSTEM_DATE_SETUP;
            } else {
                s_ui.selected_time_field_index = 0u;
                s_ui.current_screen = UI_SCREEN_SYSTEM_TIME_SETUP;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_DATE_SETUP) {
            if (s_ui.selected_date_field_index < 2u) {
                ++s_ui.selected_date_field_index;
            } else {
                s_ui.selected_date_field_index = 0u;
                s_ui.current_screen = UI_SCREEN_SYSTEM_TIME;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME_SETUP) {
            if (s_ui.selected_time_field_index < 1u) {
                ++s_ui.selected_time_field_index;
            } else {
                s_ui.selected_time_field_index = 0u;
                s_ui.current_screen = UI_SCREEN_SYSTEM_TIME;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER) {
            s_ui.current_screen = ui_get_selected_installer_item()->screen;
            if (s_ui.current_screen == UI_SCREEN_INSTALLER_FACTORY_RESET) {
                s_ui.selected_factory_reset_index = 1u;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT) {
            s_ui.current_screen = ui_get_selected_room_thermostat_item()->screen;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER_ROOM_THERMOSTAT;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_CONFIG) {
            s_ui.current_screen = ui_get_selected_io_config_item()->screen;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_SETUP) {
            s_ui.current_screen = ui_get_selected_network_setup_item()->screen;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SETUP) {
            s_ui.current_screen = ui_get_selected_wifi_setup_item()->screen;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SERVICE) {
            s_ui.current_screen = ui_get_selected_service_item()->screen;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_DELAY) {
            s_ui.delay_edit_active = !s_ui.delay_edit_active;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_FACTORY_RESET) {
            if (s_ui.selected_factory_reset_index == 0u) {
                apply_factory_reset();
            }
            s_ui.current_screen = UI_SCREEN_INSTALLER;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONES) {
            s_ui.selected_zone_detail_index = 0u;
            s_ui.current_screen = UI_SCREEN_ZONE_DETAIL;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_DETAIL) {
            switch (ui_get_selected_zone_detail_item()->id) {
            case ZONE_DETAIL_CONNECTION:
                s_ui.selected_zone_connection_index = ui_get_selected_zone_state()->paired ? 1u : 0u;
                s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
                break;
            case ZONE_DETAIL_TEMP_SOURCE:
                s_ui.selected_zone_temp_source_index = (ui_get_selected_zone_state()->temp_source == ZONE_TEMP_SOURCE_CHRONOGRAM) ? 1u : 0u;
                s_ui.current_screen = UI_SCREEN_ZONE_TEMP_SOURCE;
                break;
            case ZONE_DETAIL_CHRONOGRAM:
            default:
                s_ui.selected_chronogram_index = 0u;
                s_ui.current_screen = UI_SCREEN_ZONE_CHRONOGRAM;
                break;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_CONNECTION) {
            zone_state_t *zone = ui_get_selected_zone_state();
            if (s_ui.selected_zone_connection_index == 0u) {
                s_ui.selected_zone_sync_confirm_index = 1u;
                s_ui.current_screen = UI_SCREEN_ZONE_SYNC_CONFIRM;
            } else {
                uart_send_zone_unpair_request(ui_get_selected_zone_number());
                zone->paired = false;
                zone->link_up = false;
                zone->tx_id = 0u;
                zone->rssi_percent = 0u;
                zone->current_temp_c = 0.0f;
                zone->thermostat_target_temp_c = 0.0f;
                zone->target_temp_c = 0.0f;
                zone->output_on = false;
                redraw = true;
            }
            zones_sync_live_state_from_controller();
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_CONFIRM) {
            if (s_ui.selected_zone_sync_confirm_index == 0u) {
                zone_state_t *zone = ui_get_selected_zone_state();
                zone->paired = false;
                zone->link_up = false;
                zone->tx_id = 0u;
                zone->rssi_percent = 0u;
                zone->current_temp_c = 0.0f;
                zone->thermostat_target_temp_c = 0.0f;
                zone->target_temp_c = 0.0f;
                zone->output_on = false;
                uart_send_zone_sync_request(ui_get_selected_zone_number());
                s_ui.zone_sync_wait_deadline_ms = ui_now_ms() + ZONE_SYNC_WAIT_MS;
                s_ui.current_screen = UI_SCREEN_ZONE_SYNC_WAIT;
            } else {
                s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
            }
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_TEMP_SOURCE) {
            zone_state_t *zone = ui_get_selected_zone_state();
            zone->temp_source = (s_ui.selected_zone_temp_source_index == 0u) ? ZONE_TEMP_SOURCE_THERMOSTAT : ZONE_TEMP_SOURCE_CHRONOGRAM;
            if (zone_refresh_effective_target(zone)) {
                s_home_live_update_pending = true;
            }
            redraw = true;
        }
        break;

    case PIN_BTN_EXIT:
        if ((s_ui.current_screen == UI_SCREEN_HOME) && s_ui.root_menu_active) {
            ui_deactivate_root_menu();
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_SYSTEM_DISPLAY) ||
                   (s_ui.current_screen == UI_SCREEN_SYSTEM_LANGUAGE)) {
            s_ui.current_screen = UI_SCREEN_SYSTEM;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_SYSTEM_DATE_SETUP) ||
                   (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME_SETUP)) {
            s_ui.current_screen = UI_SCREEN_SYSTEM_TIME;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM_TIME) {
            s_ui.current_screen = UI_SCREEN_SYSTEM;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_DHCP) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_IP_ADDRESS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_SUBNET_MASK) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_GATEWAY) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_DNS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_HOSTNAME) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_LINK_STATUS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_MAC_ADDRESS)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER_NETWORK_SETUP;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_ENABLE) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SSID) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_PASSWORD) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_DHCP) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_IP_ADDRESS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_GATEWAY) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_STATUS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SIGNAL)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER_WIFI_SETUP;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_MANUAL_CONTROL) {
            s_ui.current_screen = UI_SCREEN_SERVICE;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_IO_ZONE_RELAYS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_DRY_CONTACT_RELAY) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_ZONE_AUX_SENSORS)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER_IO_CONFIG;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER_ROOM_THERMOSTAT;
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_INSTALLER_ROOM_THERMOSTAT) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_IO_CONFIG) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_NETWORK_SETUP) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_WIFI_SETUP) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_OUTDOOR_SENSOR) ||
                   (s_ui.current_screen == UI_SCREEN_INSTALLER_FACTORY_RESET)) {
            s_ui.current_screen = UI_SCREEN_INSTALLER;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER_DELAY) {
            if (s_ui.delay_edit_active) {
                s_ui.delay_edit_active = false;
            } else {
                s_ui.current_screen = UI_SCREEN_INSTALLER;
            }
            redraw = true;
        } else if ((s_ui.current_screen == UI_SCREEN_ZONE_CONNECTION) ||
                   (s_ui.current_screen == UI_SCREEN_ZONE_TEMP_SOURCE) ||
                   (s_ui.current_screen == UI_SCREEN_ZONE_CHRONOGRAM)) {
            s_ui.current_screen = UI_SCREEN_ZONE_DETAIL;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_CONFIRM) {
            s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_WAIT) {
            uart_send_zone_unpair_request(ui_get_selected_zone_number());
            s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SYSTEM) {
            ui_show_home_preserve_selection();
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_INSTALLER) {
            ui_show_home_preserve_selection();
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_SERVICE) {
            ui_show_home_preserve_selection();
            redraw = true;
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_DETAIL) {
            s_ui.current_screen = UI_SCREEN_ZONES;
            redraw = true;
        } else if (s_ui.current_screen != UI_SCREEN_HOME) {
            ui_show_home_preserve_selection();
            redraw = true;
        }
        break;

    default:
        break;
    }

    if (redraw) {
        draw_current_screen();
    }
}

static void ui_button_task(void *arg)
{
    typedef struct {
        gpio_num_t pin;
        bool last_pressed;
        TickType_t last_event_tick;
    } button_state_t;

    button_state_t buttons[] = {
        {.pin = PIN_BTN_UP,   .last_pressed = false, .last_event_tick = 0},
        {.pin = PIN_BTN_DOWN, .last_pressed = false, .last_event_tick = 0},
        {.pin = PIN_BTN_MENU, .last_pressed = false, .last_event_tick = 0},
        {.pin = PIN_BTN_EXIT, .last_pressed = false, .last_event_tick = 0},
    };

    (void)arg;

    while (1) {
        const TickType_t now = xTaskGetTickCount();

        for (size_t i = 0; i < (sizeof(buttons) / sizeof(buttons[0])); ++i) {
            const bool pressed = button_is_pressed(buttons[i].pin);

            if (pressed && !buttons[i].last_pressed &&
                (now - buttons[i].last_event_tick) >= pdMS_TO_TICKS(UI_BUTTON_HOLD_OFF_MS)) {
                buttons[i].last_event_tick = now;
                ui_handle_button_press(buttons[i].pin);
            }

            buttons[i].last_pressed = pressed;
        }

        vTaskDelay(pdMS_TO_TICKS(UI_BUTTON_POLL_MS));
    }
}

static int text_width5x7(const char *text, int scale)
{
    int width = 0;
    const size_t len = strlen(text);

    for (size_t i = 0; i < len; ++i) {
        width += (5 * scale);
        if (i + 1 < len) {
            width += scale;
        }
    }
    return width;
}

static void draw_char5x7_rot90cw(int x, int y, int scale, char ch, uint16_t color)
{
    const uint8_t *rows = font5x7_get(ch);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] & (1u << (4 - col))) != 0u) {
                lcd_fill_rect_rot90cw(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text5x7_rot90cw(int x, int y, int scale, const char *text, uint16_t color)
{
    int cursor_x = x;

    for (size_t i = 0; text[i] != '\0'; ++i) {
        draw_char5x7_rot90cw(cursor_x, y, scale, text[i], color);
        cursor_x += 5 * scale + scale;
    }
}

static void draw_centered_text5x7_rot90cw(int x, int y, int w, int h, int scale, const char *text, uint16_t color)
{
    const int text_w = text_width5x7(text, scale);
    const int text_h = 7 * scale;
    const int draw_x = x + (w - text_w) / 2;
    const int draw_y = y + (h - text_h) / 2;

    draw_text5x7_rot90cw(draw_x, draw_y, scale, text, color);
}

static void draw_centered_bold_text5x7_rot90cw(int x, int y, int w, int h, int scale, const char *text, uint16_t color)
{
    const int text_w = text_width5x7(text, scale);
    const int text_h = 7 * scale;
    const int draw_x = x + (w - text_w) / 2;
    const int draw_y = y + (h - text_h) / 2;

    draw_text5x7_rot90cw(draw_x, draw_y, scale, text, color);
    draw_text5x7_rot90cw(draw_x + 1, draw_y, scale, text, color);
}

static float clamp_hysteresis_value(float value)
{
    if (value < HYSTERESIS_MIN_C) {
        return HYSTERESIS_MIN_C;
    }
    if (value > HYSTERESIS_MAX_C) {
        return HYSTERESIS_MAX_C;
    }
    return value;
}

static float clamp_calibration_value(float value)
{
    if (value < CALIBRATION_MIN_C) {
        return CALIBRATION_MIN_C;
    }
    if (value > CALIBRATION_MAX_C) {
        return CALIBRATION_MAX_C;
    }
    return value;
}

static float calibrated_temperature_c(float raw_temp_c)
{
    float calibration;

    if (raw_temp_c <= 0.0f) {
        return raw_temp_c;
    }

    state_lock();
    calibration = s_state.temp_calibration_c;
    state_unlock();
    return raw_temp_c + calibration;
}

static void draw_circle_ring_rot90cw(int cx, int cy, int radius, int thickness, uint16_t color)
{
    const int outer = radius + thickness;
    const int outer2 = outer * outer;
    const int inner = (radius > thickness) ? (radius - thickness) : 0;
    const int inner2 = inner * inner;

    for (int dy = -outer; dy <= outer; ++dy) {
        for (int dx = -outer; dx <= outer; ++dx) {
            const int d2 = dx * dx + dy * dy;
            if ((d2 <= outer2) && (d2 >= inner2)) {
                lcd_fill_rect_rot90cw(cx + dx, cy + dy, 1, 1, color);
            }
        }
    }
}

static void draw_degree_symbol_rot90cw(int x, int y, int size, uint16_t color)
{
    lcd_draw_rect_rot90cw(x, y, size, size, 1, color);
}

static void draw_temp_string_rot90cw(int x, int y, int scale, const char *value, uint16_t color)
{
    const int width = text_width5x7(value, scale);

    draw_text5x7_rot90cw(x, y, scale, value, color);
    draw_degree_symbol_rot90cw(x + width + scale, y, scale + 1, color);
}

static void draw_centered_temp_string_rot90cw(int x, int y, int w, int h, int scale, const char *value, uint16_t color)
{
    const int text_w = text_width5x7(value, scale) + scale + (scale + 1);
    const int text_h = 7 * scale;
    const int draw_x = x + (w - text_w) / 2;
    const int draw_y = y + (h - text_h) / 2;

    draw_temp_string_rot90cw(draw_x, draw_y, scale, value, color);
}

static void draw_progress_bar_rot90cw(int x, int y, int w, int h, int percent)
{
    const uint16_t fg = 0x0000;
    const uint16_t bg = 0xFFFF;
    const int inner_w = w - 4;
    const int fill_w = (inner_w * percent) / 100;

    lcd_draw_rect_rot90cw(x, y, w, h, 2, fg);
    lcd_fill_rect_rot90cw(x + 2, y + 2, inner_w, h - 4, bg);
    if (fill_w > 0) {
        lcd_fill_rect_rot90cw(x + 2, y + 2, fill_w, h - 4, fg);
    }
}

static void draw_sun_icon_rot90cw(int cx, int cy, uint16_t color)
{
    lcd_fill_rect_rot90cw(cx - 4, cy - 4, 8, 8, color);
    lcd_draw_thick_line_rot90cw(cx - 8, cy, cx - 12, cy, 1, color);
    lcd_draw_thick_line_rot90cw(cx + 8, cy, cx + 12, cy, 1, color);
    lcd_draw_thick_line_rot90cw(cx, cy - 8, cx, cy - 12, 1, color);
    lcd_draw_thick_line_rot90cw(cx, cy + 8, cx, cy + 12, 1, color);
    lcd_draw_thick_line_rot90cw(cx - 6, cy - 6, cx - 10, cy - 10, 1, color);
    lcd_draw_thick_line_rot90cw(cx + 6, cy - 6, cx + 10, cy - 10, 1, color);
    lcd_draw_thick_line_rot90cw(cx - 6, cy + 6, cx - 10, cy + 10, 1, color);
    lcd_draw_thick_line_rot90cw(cx + 6, cy + 6, cx + 10, cy + 10, 1, color);
}

static void draw_droplet_icon_rot90cw(int x, int y, uint16_t color)
{
    lcd_fill_triangle_rot90cw(x + 5, y, x, y + 8, x + 10, y + 8, color);
    lcd_fill_rect_rot90cw(x + 2, y + 8, 6, 7, color);
}

static void draw_status_square_rot90cw(int x, int y, bool on)
{
    const uint16_t fg = 0x0000;
    const uint16_t bg = 0xFFFF;

    lcd_draw_rect_rot90cw(x, y, 8, 8, 1, fg);
    if (on) {
        lcd_fill_rect_rot90cw(x + 2, y + 2, 4, 4, fg);
    } else {
        lcd_fill_rect_rot90cw(x + 1, y + 1, 6, 6, bg);
    }
}

static void draw_splash_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const int src_w = UI_LOGICAL_WIDTH;
    const int logo_w = 108;
    const int logo_h = 50;
    const int logo_x = (src_w - logo_w) / 2;
    const int logo_y = 6;
    const int ajax_cell_w = 22;
    const int ajax_cell_h = 24;
    const int ajax_gap_x = 2;
    const int ajax_start_x = (src_w - (ajax_cell_w * 4 + ajax_gap_x * 3)) / 2;
    const int ajax_start_y = logo_y + logo_h + 6;
    const int klim_cell_w = 18;
    const int klim_cell_h = 22;
    const int klim_gap_x = 3;
    const int klim_start_x = (src_w - (klim_cell_w * 4 + klim_gap_x * 3)) / 2;
    const int klim_start_y = ajax_start_y + ajax_cell_h + 8;

    lcd_fill_screen(bg);
    draw_logo_rot90cw(logo_x, logo_y, logo_w, logo_h, fg);

    draw_letter_a_rot90cw(ajax_start_x, ajax_start_y, ajax_cell_w, ajax_cell_h, fg);
    draw_letter_j_rot90cw(ajax_start_x + ajax_cell_w + ajax_gap_x, ajax_start_y, ajax_cell_w, ajax_cell_h, fg);
    draw_letter_a_rot90cw(ajax_start_x + (ajax_cell_w + ajax_gap_x) * 2, ajax_start_y, ajax_cell_w, ajax_cell_h, fg);
    draw_letter_x_rot90cw(ajax_start_x + (ajax_cell_w + ajax_gap_x) * 3, ajax_start_y, ajax_cell_w, ajax_cell_h, fg);

    draw_glyph_rot90cw(klim_start_x, klim_start_y, klim_cell_w, klim_cell_h, GLYPH_K, fg);
    draw_glyph_rot90cw(klim_start_x + klim_cell_w + klim_gap_x, klim_start_y, klim_cell_w, klim_cell_h, GLYPH_L, fg);
    draw_glyph_rot90cw(klim_start_x + (klim_cell_w + klim_gap_x) * 2, klim_start_y, klim_cell_w, klim_cell_h, GLYPH_I, fg);
    draw_glyph_rot90cw(klim_start_x + (klim_cell_w + klim_gap_x) * 3, klim_start_y, klim_cell_w, klim_cell_h, GLYPH_M, fg);
}

static void run_boot_sequence(void)
{
    const int steps = BOOT_SPLASH_DURATION_MS / BOOT_SPLASH_STEP_MS;
    const int bar_x = 20;
    const int bar_y = 112;
    const int bar_w = 120;
    const int bar_h = 12;

    draw_splash_screen();
    lcd_present();

    for (int i = 0; i <= steps; ++i) {
        const int percent = (i * 100) / steps;
        draw_progress_bar_rot90cw(bar_x, bar_y, bar_w, bar_h, percent);
        lcd_present();
        if (i < steps) {
            vTaskDelay(pdMS_TO_TICKS(BOOT_SPLASH_STEP_MS));
        }
    }
}

static inline void ds1302_delay(void)
{
    esp_rom_delay_us(DS1302_IO_DELAY_US);
}

static inline void ds1302_ce_set(int level)
{
    gpio_set_level(PIN_RTC_CE, level);
}

static inline void ds1302_clk_set(int level)
{
    gpio_set_level(PIN_RTC_SCLK, level);
}

static void ds1302_io_mode_output(void)
{
    gpio_set_direction(PIN_RTC_IO, GPIO_MODE_OUTPUT);
}

static void ds1302_io_mode_input(void)
{
    gpio_set_direction(PIN_RTC_IO, GPIO_MODE_INPUT);
}

static void ds1302_io_write(int level)
{
    ds1302_io_mode_output();
    gpio_set_level(PIN_RTC_IO, level);
}

static int ds1302_io_read(void)
{
    return gpio_get_level(PIN_RTC_IO);
}

static void ds1302_write_byte(uint8_t value)
{
    ds1302_io_mode_output();

    for (int i = 0; i < 8; ++i) {
        ds1302_clk_set(0);
        ds1302_io_write((value >> i) & 0x1u);
        ds1302_delay();
        ds1302_clk_set(1);
        ds1302_delay();
    }

    ds1302_clk_set(0);
}

static uint8_t ds1302_read_byte(void)
{
    uint8_t value = 0u;

    ds1302_io_mode_input();
    for (int i = 0; i < 8; ++i) {
        ds1302_clk_set(0);
        ds1302_delay();
        if (ds1302_io_read()) {
            value |= (uint8_t)(1u << i);
        }
        ds1302_clk_set(1);
        ds1302_delay();
    }

    ds1302_clk_set(0);
    return value;
}

static uint8_t ds1302_read_reg(uint8_t cmd_read)
{
    uint8_t value;

    ds1302_ce_set(1);
    ds1302_delay();
    ds1302_write_byte(cmd_read);
    value = ds1302_read_byte();
    ds1302_ce_set(0);
    ds1302_delay();
    ds1302_io_mode_input();
    return value;
}

static void ds1302_write_reg(uint8_t cmd_write, uint8_t value)
{
    ds1302_ce_set(1);
    ds1302_delay();
    ds1302_write_byte(cmd_write);
    ds1302_write_byte(value);
    ds1302_ce_set(0);
    ds1302_delay();
    ds1302_io_mode_input();
}

static uint8_t ds1302_bcd_to_bin(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10u) + (value & 0x0Fu));
}

static uint8_t ds1302_bin_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static bool ds1302_read_datetime(struct tm *tm_value)
{
    const uint8_t seconds = ds1302_read_reg(0x81u);
    const uint8_t minutes = ds1302_read_reg(0x83u);
    const uint8_t hours = ds1302_read_reg(0x85u);
    const uint8_t date = ds1302_read_reg(0x87u);
    const uint8_t month = ds1302_read_reg(0x89u);
    const uint8_t year = ds1302_read_reg(0x8Du);

    if ((seconds & 0x80u) != 0u) {
        return false;
    }

    tm_value->tm_sec = ds1302_bcd_to_bin(seconds & 0x7Fu);
    tm_value->tm_min = ds1302_bcd_to_bin(minutes & 0x7Fu);
    tm_value->tm_hour = ds1302_bcd_to_bin(hours & 0x3Fu);
    tm_value->tm_mday = ds1302_bcd_to_bin(date & 0x3Fu);
    tm_value->tm_mon = (int)ds1302_bcd_to_bin(month & 0x1Fu) - 1;
    tm_value->tm_year = (int)ds1302_bcd_to_bin(year) + 100;
    tm_value->tm_isdst = -1;

    if ((tm_value->tm_sec > 59) || (tm_value->tm_min > 59) || (tm_value->tm_hour > 23) ||
        (tm_value->tm_mday < 1) || (tm_value->tm_mday > 31) ||
        (tm_value->tm_mon < 0) || (tm_value->tm_mon > 11) ||
        (tm_value->tm_year < 100)) {
        return false;
    }

    return mktime(tm_value) != (time_t)-1;
}

static void ds1302_write_datetime(const struct tm *tm_value)
{
    const int ds_day = (tm_value->tm_wday == 0) ? 7 : tm_value->tm_wday;

    ds1302_write_reg(0x8Eu, 0x00u);
    ds1302_write_reg(0x90u, 0x00u);
    ds1302_write_reg(0x80u, ds1302_bin_to_bcd((uint8_t)tm_value->tm_sec) & 0x7Fu);
    ds1302_write_reg(0x82u, ds1302_bin_to_bcd((uint8_t)tm_value->tm_min));
    ds1302_write_reg(0x84u, ds1302_bin_to_bcd((uint8_t)tm_value->tm_hour) & 0x3Fu);
    ds1302_write_reg(0x86u, ds1302_bin_to_bcd((uint8_t)tm_value->tm_mday));
    ds1302_write_reg(0x88u, ds1302_bin_to_bcd((uint8_t)(tm_value->tm_mon + 1)));
    ds1302_write_reg(0x8Au, ds1302_bin_to_bcd((uint8_t)ds_day));
    ds1302_write_reg(0x8Cu, ds1302_bin_to_bcd((uint8_t)(tm_value->tm_year % 100)));
    ds1302_write_reg(0x8Eu, 0x80u);
}

static void ds1302_init_pins(void)
{
    gpio_config_t rtc_out = {
        .pin_bit_mask = (1ULL << PIN_RTC_CE) | (1ULL << PIN_RTC_SCLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t rtc_io = {
        .pin_bit_mask = (1ULL << PIN_RTC_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&rtc_out));
    ESP_ERROR_CHECK(gpio_config(&rtc_io));
    ds1302_ce_set(0);
    ds1302_clk_set(0);
}

static bool clock_is_leap_year(int year)
{
    return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}

static int clock_days_in_month(int year, int month)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    if ((month < 1) || (month > 12)) {
        return 31;
    }

    if ((month == 2) && clock_is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

static void clock_get_tm(struct tm *tm_value)
{
    time_t now = s_clock_base_epoch + (time_t)((esp_timer_get_time() - s_clock_base_us) / 1000000LL);
    localtime_r(&now, tm_value);
    tm_value->tm_isdst = -1;
}

static void clock_set_from_tm(const struct tm *tm_input)
{
    struct tm tm_value = *tm_input;

    tm_value.tm_isdst = -1;
    s_clock_base_epoch = mktime(&tm_value);
    s_clock_base_us = esp_timer_get_time();
    localtime_r(&s_clock_base_epoch, &tm_value);
    ds1302_write_datetime(&tm_value);
    s_home_live_update_pending = true;
}

static void clock_load_edit_tm(void)
{
    clock_get_tm(&s_clock_edit_tm);
}

static void clock_adjust_date_field(int step)
{
    int year = s_clock_edit_tm.tm_year + 1900;
    int month = s_clock_edit_tm.tm_mon + 1;
    int day = s_clock_edit_tm.tm_mday;

    if (s_ui.selected_date_field_index == 0u) {
        const int max_day = clock_days_in_month(year, month);
        day += step;
        if (day < 1) {
            day = max_day;
        } else if (day > max_day) {
            day = 1;
        }
    } else if (s_ui.selected_date_field_index == 1u) {
        month += step;
        if (month < 1) {
            month = 12;
        } else if (month > 12) {
            month = 1;
        }
        if (day > clock_days_in_month(year, month)) {
            day = clock_days_in_month(year, month);
        }
    } else {
        year += step;
        if (year < 2026) {
            year = 2046;
        } else if (year > 2046) {
            year = 2026;
        }
        if (day > clock_days_in_month(year, month)) {
            day = clock_days_in_month(year, month);
        }
    }

    s_clock_edit_tm.tm_year = year - 1900;
    s_clock_edit_tm.tm_mon = month - 1;
    s_clock_edit_tm.tm_mday = day;
    clock_set_from_tm(&s_clock_edit_tm);
}

static void clock_adjust_time_field(int step)
{
    int hour = s_clock_edit_tm.tm_hour;
    int minute = s_clock_edit_tm.tm_min;

    if (s_ui.selected_time_field_index == 0u) {
        hour += step;
        if (hour < 0) {
            hour = 23;
        } else if (hour > 23) {
            hour = 0;
        }
    } else {
        minute += step;
        if (minute < 0) {
            minute = 59;
        } else if (minute > 59) {
            minute = 0;
        }
    }

    s_clock_edit_tm.tm_hour = hour;
    s_clock_edit_tm.tm_min = minute;
    s_clock_edit_tm.tm_sec = 0;
    clock_set_from_tm(&s_clock_edit_tm);
}

static int month_from_abbr(const char *abbr)
{
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    for (int i = 0; i < 12; ++i) {
        if (strcmp(abbr, months[i]) == 0) {
            return i + 1;
        }
    }
    return 1;
}

static const char *weekday_abbr_from_tm_wday(int wday)
{
    static const char *days[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };

    if ((wday < 0) || (wday > 6)) {
        return "MON";
    }
    return days[wday];
}

static void clock_set_date_midnight(int year, int month, int day)
{
    struct tm tm_value = {0};

    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = 0;
    tm_value.tm_min = 0;
    tm_value.tm_sec = 0;
    clock_set_from_tm(&tm_value);
}

static void clock_init_factory_default(void)
{
    struct tm tm_value = {0};
    char month[4] = {0};
    int day = 1;
    int year = 2026;

    if (ds1302_read_datetime(&tm_value)) {
        clock_set_from_tm(&tm_value);
        ESP_LOGI(TAG, "DS1302 time loaded: %04d-%02d-%02d %02d:%02d:%02d",
                 tm_value.tm_year + 1900, tm_value.tm_mon + 1, tm_value.tm_mday,
                 tm_value.tm_hour, tm_value.tm_min, tm_value.tm_sec);
        return;
    }

    if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
        strcpy(month, "Jan");
        day = 1;
        year = 2026;
    }

    clock_set_date_midnight(year, month_from_abbr(month), day);
}

static void clock_reset_factory_default(void)
{
    time_t now = s_clock_base_epoch + (time_t)((esp_timer_get_time() - s_clock_base_us) / 1000000LL);
    struct tm tm_value = {0};

    localtime_r(&now, &tm_value);
    clock_set_date_midnight(tm_value.tm_year + 1900, tm_value.tm_mon + 1, tm_value.tm_mday);
}

static void clock_get_display_strings(char *date_buf, size_t date_buf_len, char *time_buf, size_t time_buf_len)
{
    struct tm tm_value = {0};
    const char *weekday;

    clock_get_tm(&tm_value);
    const unsigned day =  (unsigned)tm_value.tm_mday;
    const unsigned month = (unsigned)(tm_value.tm_mon + 1);
    const unsigned year = (unsigned)(tm_value.tm_year + 1900);
    const unsigned hour = (unsigned)tm_value.tm_hour;
    const unsigned minute = (unsigned)tm_value.tm_min;
    weekday = weekday_abbr_from_tm_wday(tm_value.tm_wday);
    snprintf(date_buf, date_buf_len, "%02u.%02u.%04u %s", day, month, year, weekday);
    snprintf(time_buf, time_buf_len, "%02u:%02u", hour, minute);
}

static int clock_current_minutes_of_day(void)
{
    struct tm tm_value = {0};

    clock_get_tm(&tm_value);
    return tm_value.tm_hour * 60 + tm_value.tm_min;
}

static bool zone_schedule_entry_is_active(const zone_schedule_entry_t *entry, int now_minutes)
{
    const int start = (int)entry->start_hour * 60 + (int)entry->start_minute;
    const int end = (int)entry->end_hour * 60 + (int)entry->end_minute;

    if (start == end) {
        return true;
    }
    if (start < end) {
        return (now_minutes >= start) && (now_minutes <= end);
    }
    return (now_minutes >= start) || (now_minutes <= end);
}

static float zone_resolve_effective_target_temp(const zone_state_t *zone)
{
    if (zone->temp_source == ZONE_TEMP_SOURCE_CHRONOGRAM) {
        const int now_minutes = clock_current_minutes_of_day();

        for (size_t i = 0; i < 3u; ++i) {
            const zone_schedule_entry_t *entry = &zone->schedule[i];
            if (zone_schedule_entry_is_active(entry, now_minutes) && (entry->target_temp_c > 0u)) {
                return clamp_target_temp((float)entry->target_temp_c);
            }
        }
    }

    if (zone->thermostat_target_temp_c > 0.0f) {
        return clamp_target_temp(zone->thermostat_target_temp_c);
    }

    if (zone->base_set_temp_c > 0u) {
        return clamp_target_temp((float)zone->base_set_temp_c);
    }

    return 0.0f;
}

static bool zone_refresh_effective_target(zone_state_t *zone)
{
    const float resolved = zone_resolve_effective_target_temp(zone);

    if ((zone->target_temp_c < (resolved - 0.05f)) || (zone->target_temp_c > (resolved + 0.05f))) {
        zone->target_temp_c = resolved;
        return true;
    }
    return false;
}

static bool zone_logic_is_active(const zone_state_t *zone)
{
    float hysteresis;
    float diff;
    float current_temp;

    if (!zone->paired) {
        return false;
    }

    state_lock();
    hysteresis = s_state.auto_hysteresis_c;
    state_unlock();

    if (hysteresis <= 0.0f) {
        hysteresis = AUTO_HYSTERESIS_DEFAULT_C;
    }

    current_temp = calibrated_temperature_c(zone->current_temp_c);
    diff = current_temp - zone->target_temp_c;

    switch (zone->mode) {
    case HVAC_MODE_COOL:
        if ((current_temp <= 0.0f) || (zone->target_temp_c <= 0.0f)) {
            return false;
        }
        return diff > 0.0f;
    case HVAC_MODE_HEAT:
        if ((current_temp <= 0.0f) || (zone->target_temp_c <= 0.0f)) {
            return false;
        }
        return diff < 0.0f;
    case HVAC_MODE_AUTO:
        if ((current_temp <= 0.0f) || (zone->target_temp_c <= 0.0f)) {
            return false;
        }
        return (diff >= hysteresis) || (diff <= -hysteresis);
    case HVAC_MODE_FAN_ONLY:
        return true;
    case HVAC_MODE_OFF:
    default:
        return false;
    }
}

static uint16_t zone_tile_fill_color(const zone_state_t *zone)
{
    if (!zone_logic_is_active(zone)) {
        return 0xFFFF;
    }

    switch (zone->mode) {
    case HVAC_MODE_COOL:
        return 0x001F;
    case HVAC_MODE_HEAT:
        return 0xF800;
    case HVAC_MODE_AUTO:
        return 0xFFE0;
    case HVAC_MODE_FAN_ONLY:
        return 0xC618;
    case HVAC_MODE_OFF:
    default:
        return 0xFFFF;
    }
}

static bool zone_tile_is_highlighted(const zone_state_t *zone)
{
    return zone_logic_is_active(zone);
}

static uint16_t zone_tile_text_color(const zone_state_t *zone)
{
    const uint16_t fill = zone_tile_fill_color(zone);

    if ((fill == 0xF800) || (fill == 0x001F)) {
        return 0xFFFF;
    }

    return 0x0000;
}

static void draw_dashboard_zone_row(void)
{
    const uint16_t fg = 0x0000;
    const size_t visible_zone_count = ui_dashboard_visible_zone_count();
    const bool have_selected_zone = ui_sync_dashboard_selected_zone();
    const size_t selected_zone_index = have_selected_zone ? s_ui.selected_zone_index : ZONE_MAX_COUNT;
    const int zone_gap = 1;
    const int zone_box_w = 19;
    const int zone_box_h = 24;
    const int zone_total_w = (int)DASHBOARD_ZONE_SLOTS * zone_box_w + ((int)DASHBOARD_ZONE_SLOTS - 1) * zone_gap;
    const int zone_start_x = (160 - zone_total_w) / 2;

    for (size_t i = 0; i < DASHBOARD_ZONE_SLOTS; ++i) {
        const int x = zone_start_x + (int)i * (zone_box_w + zone_gap);
        const int y = 20;

        if (i < visible_zone_count) {
            const size_t zone_index = s_ui.dashboard_zone_scroll + i;
            const zone_state_t *zone = &s_zones[zone_index];

            if (zone->paired) {
                const bool selected = (zone_index == selected_zone_index);
                const bool highlighted = zone_tile_is_highlighted(zone);
                const uint16_t tile_fill = zone_tile_fill_color(zone);
                const uint16_t tile_text = zone_tile_text_color(zone);
                const int border_thickness = selected ? 2 : 1;
                char num[4];

                if (highlighted) {
                    lcd_fill_rect_rot90cw(x + border_thickness, y + border_thickness,
                                          zone_box_w - border_thickness * 2,
                                          zone_box_h - border_thickness * 2,
                                          tile_fill);
                }

                lcd_draw_rect_rot90cw(x, y, zone_box_w, zone_box_h, border_thickness, fg);
                snprintf(num, sizeof(num), "%u", (unsigned)(zone_index + 1u));
                draw_centered_text5x7_rot90cw(x, y, zone_box_w, zone_box_h, 2, num, highlighted ? tile_text : fg);
                continue;
            }
        }

        lcd_draw_rect_rot90cw(x, y, zone_box_w, zone_box_h, 1, fg);
    }
}

static void draw_dashboard_screen(const klim_state_t *state)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const char *mode_label = mode_to_ui_label(state->mode);
    const size_t menu_count = ui_root_item_count();
    const bool show_selection = s_ui.root_menu_active;
    char date_text[16];
    char clock_text[8];
    const zone_state_t *dashboard_zone = NULL;
    char temp_buf[16];
    char set_buf[16];
    char rssi_buf[8];

    if (ui_sync_dashboard_selected_zone()) {
        dashboard_zone = &s_zones[s_ui.selected_zone_index];
    }

    if (dashboard_zone == NULL) {
        for (size_t i = 0; i < ZONE_MAX_COUNT; ++i) {
            if (s_zones[i].paired) {
                dashboard_zone = &s_zones[i];
                break;
            }
        }
    }

    clock_get_display_strings(date_text, sizeof(date_text), clock_text, sizeof(clock_text));
    if ((dashboard_zone != NULL) && (dashboard_zone->current_temp_c > 0.0f)) {
        snprintf(temp_buf, sizeof(temp_buf), "%.1f", calibrated_temperature_c(dashboard_zone->current_temp_c));
    } else {
        strcpy(temp_buf, "00");
    }
    if ((dashboard_zone != NULL) && (dashboard_zone->target_temp_c > 0.0f)) {
        snprintf(set_buf, sizeof(set_buf), "%.0f", dashboard_zone->target_temp_c);
    } else {
        strcpy(set_buf, "00");
    }
    {
        unsigned rssi_percent = 0u;

        if (dashboard_zone != NULL) {
            if (dashboard_zone->rssi_percent > 100u) {
                rssi_percent = 100u;
            } else {
                rssi_percent = (unsigned)dashboard_zone->rssi_percent;
            }
        }
        snprintf(rssi_buf, sizeof(rssi_buf), "%u%%", rssi_percent);
    }

    if (show_selection) {
        const int box_x = 0;
        const int box_y = 0;
        const int box_w = 160;
        const int box_h = 128;

        lcd_fill_rect_rot90cw(box_x, box_y, box_w, box_h, bg);
        lcd_draw_rect_rot90cw(box_x, box_y, box_w, box_h, 2, fg);
        lcd_fill_rect_rot90cw(box_x, box_y, box_w, 18, fg);
        draw_centered_text5x7_rot90cw(box_x, box_y, box_w, 18, 2, "MENU", bg);

        for (size_t i = 0; i < menu_count; ++i) {
            const int row_x = box_x + 8;
            const int row_y = box_y + 24 + (int)i * 18;
            const int row_w = box_w - 16;
            const bool selected = (i == s_ui.selected_root_index);
            char line[20];

            snprintf(line, sizeof(line), "%u %s", s_root_menu_items[i].slot, s_root_menu_items[i].title);
            lcd_draw_rect_rot90cw(row_x, row_y, row_w, 14, 1, fg);
            if (selected) {
                lcd_fill_rect_rot90cw(row_x + 1, row_y + 1, row_w - 2, 12, fg);
                draw_text5x7_rot90cw(row_x + 5, row_y + 3, 1, line, bg);
            } else {
                draw_text5x7_rot90cw(row_x + 5, row_y + 3, 1, line, fg);
            }
        }
        return;
    }

    lcd_fill_screen(bg);

    draw_text5x7_rot90cw(4, 5, 1, date_text, fg);
    draw_text5x7_rot90cw(160 - 4 - text_width5x7(clock_text, 1), 5, 1, clock_text, fg);
    lcd_fill_rect_rot90cw(0, 17, 160, 1, fg);
    draw_dashboard_zone_row();

    lcd_draw_rect_rot90cw(0, 48, 82, 80, 1, fg);
    lcd_fill_rect_rot90cw(0, 48, 82, 12, fg);
    draw_centered_text5x7_rot90cw(0, 48, 82, 12, 1, "TEMP", bg);
    draw_centered_temp_string_rot90cw(0, 60, 82, 68, 3, temp_buf, fg);

    lcd_draw_rect_rot90cw(82, 48, 50, 38, 1, fg);
    lcd_fill_rect_rot90cw(82, 48, 50, 12, fg);
    draw_centered_text5x7_rot90cw(82, 48, 50, 12, 1, "SET", bg);
    draw_temp_string_rot90cw(90, 63, 2, set_buf, fg);

    lcd_draw_rect_rot90cw(132, 48, 28, 38, 1, fg);
    lcd_fill_rect_rot90cw(132, 48, 28, 12, fg);
    draw_centered_text5x7_rot90cw(132, 48, 28, 12, 1, "MODE", bg);
    draw_centered_bold_text5x7_rot90cw(132, 61, 28, 23, 1, mode_label, fg);

    lcd_draw_rect_rot90cw(82, 86, 78, 42, 1, fg);
    {
        const unsigned rssi_percent = (dashboard_zone != NULL) ? (unsigned)dashboard_zone->rssi_percent : 0u;
        const bool bar1_on = (rssi_percent >= 10u);
        const bool bar2_on = (rssi_percent >= 35u);
        const bool bar3_on = (rssi_percent >= 60u);
        const bool bar4_on = (rssi_percent >= 85u);

        lcd_draw_rect_rot90cw(88, 108, 6, 12, 1, fg);
        lcd_draw_rect_rot90cw(98, 104, 6, 16, 1, fg);
        lcd_draw_rect_rot90cw(108, 99, 6, 21, 1, fg);
        lcd_draw_rect_rot90cw(118, 94, 6, 26, 1, fg);
        if (bar1_on) {
            lcd_fill_rect_rot90cw(89, 109, 4, 10, fg);
        }
        if (bar2_on) {
            lcd_fill_rect_rot90cw(99, 105, 4, 14, fg);
        }
        if (bar3_on) {
            lcd_fill_rect_rot90cw(109, 100, 4, 19, fg);
        }
        if (bar4_on) {
            lcd_fill_rect_rot90cw(119, 95, 4, 24, fg);
        }
    }
    draw_text5x7_rot90cw(130, 104, 1, rssi_buf, fg);

}

static void draw_mode_screen(const klim_state_t *state)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_mode_item_count();
    const hvac_mode_t current_mode = state->mode;

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "MODE", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 28 + (int)i * 18;
        const bool selected = (i == s_ui.selected_mode_index);
        const bool active = (s_mode_menu_items[i].mode == current_mode);
        const uint16_t row_fg = selected ? bg : fg;

        lcd_draw_rect_rot90cw(x, y, 140, 16, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 14, fg);
            draw_text5x7_rot90cw(x + 6, y + 4, 1, s_mode_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 6, y + 4, 1, s_mode_menu_items[i].title, fg);
        }

        lcd_draw_rect_rot90cw(x + 124, y + 4, 8, 8, 1, row_fg);
        if (active) {
            lcd_fill_rect_rot90cw(x + 126, y + 6, 4, 4, row_fg);
        }
    }
}

static void draw_system_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_system_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "SYSTEM", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 28 + (int)i * 24;
        const bool selected = (i == s_ui.selected_system_index);

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_system_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_system_menu_items[i].title, fg);
        }
    }
}

static void draw_system_time_menu_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_system_time_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "DATE/TIME", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 34 + (int)i * 24;
        const bool selected = (i == s_ui.selected_system_time_index);

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_system_time_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_system_time_menu_items[i].title, fg);
        }
    }
}

static void draw_system_date_setup_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    char day_buf[8];
    char month_buf[8];
    char year_buf[12];
    const unsigned day = (unsigned)s_clock_edit_tm.tm_mday;
    const unsigned month = (unsigned)(s_clock_edit_tm.tm_mon + 1);
    const unsigned year = (unsigned)(s_clock_edit_tm.tm_year + 1900);

    snprintf(day_buf, sizeof(day_buf), "%02u", day);
    snprintf(month_buf, sizeof(month_buf), "%02u", month);
    snprintf(year_buf, sizeof(year_buf), "%04u", year);

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "DATE SETUP", bg);

    lcd_draw_rect_rot90cw(10, 30, 36, 28, 1, fg);
    lcd_draw_rect_rot90cw(54, 30, 36, 28, 1, fg);
    lcd_draw_rect_rot90cw(98, 30, 52, 28, 1, fg);
    if (s_ui.selected_date_field_index == 0u) {
        lcd_fill_rect_rot90cw(11, 31, 34, 26, fg);
    } else if (s_ui.selected_date_field_index == 1u) {
        lcd_fill_rect_rot90cw(55, 31, 34, 26, fg);
    } else {
        lcd_fill_rect_rot90cw(99, 31, 50, 26, fg);
    }

    draw_centered_text5x7_rot90cw(10, 34, 36, 16, 2, day_buf, (s_ui.selected_date_field_index == 0u) ? bg : fg);
    draw_centered_text5x7_rot90cw(54, 34, 36, 16, 2, month_buf, (s_ui.selected_date_field_index == 1u) ? bg : fg);
    draw_centered_text5x7_rot90cw(98, 34, 52, 16, 2, year_buf, (s_ui.selected_date_field_index == 2u) ? bg : fg);

    draw_centered_text5x7_rot90cw(10, 62, 36, 10, 1, "DAY", fg);
    draw_centered_text5x7_rot90cw(54, 62, 36, 10, 1, "MONTH", fg);
    draw_centered_text5x7_rot90cw(98, 62, 52, 10, 1, "YEAR", fg);

    lcd_draw_rect_rot90cw(10, 82, 140, 34, 1, fg);
    draw_centered_text5x7_rot90cw(14, 90, 132, 10, 1, "YEAR RANGE 2026-2046", fg);
    draw_centered_text5x7_rot90cw(14, 102, 132, 10, 1, "MENU NEXT  EXIT BACK", fg);
}

static void draw_system_time_setup_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    char hour_buf[8];
    char minute_buf[8];
    const unsigned hour = (unsigned)s_clock_edit_tm.tm_hour;
    const unsigned minute = (unsigned)s_clock_edit_tm.tm_min;

    snprintf(hour_buf, sizeof(hour_buf), "%02u", hour);
    snprintf(minute_buf, sizeof(minute_buf), "%02u", minute);

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "TIME SETUP", bg);

    lcd_draw_rect_rot90cw(22, 36, 46, 30, 1, fg);
    lcd_draw_rect_rot90cw(92, 36, 46, 30, 1, fg);
    draw_centered_text5x7_rot90cw(68, 42, 24, 14, 2, ":", fg);
    if (s_ui.selected_time_field_index == 0u) {
        lcd_fill_rect_rot90cw(23, 37, 44, 28, fg);
    } else {
        lcd_fill_rect_rot90cw(93, 37, 44, 28, fg);
    }

    draw_centered_text5x7_rot90cw(22, 42, 46, 16, 2, hour_buf, (s_ui.selected_time_field_index == 0u) ? bg : fg);
    draw_centered_text5x7_rot90cw(92, 42, 46, 16, 2, minute_buf, (s_ui.selected_time_field_index == 1u) ? bg : fg);

    draw_centered_text5x7_rot90cw(22, 70, 46, 10, 1, "HOUR", fg);
    draw_centered_text5x7_rot90cw(92, 70, 46, 10, 1, "MIN", fg);

    lcd_draw_rect_rot90cw(10, 88, 140, 28, 1, fg);
    draw_centered_text5x7_rot90cw(14, 96, 132, 10, 1, "MENU NEXT  EXIT BACK", fg);
}

static void draw_installer_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_installer_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "INSTALLER MENU", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 8;
        const int y = 26 + (int)i * 12;
        const bool selected = (i == s_ui.selected_installer_index);

        lcd_draw_rect_rot90cw(x, y, 144, 10, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 142, 8, fg);
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_installer_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_installer_menu_items[i].title, fg);
        }
    }
}

static void draw_room_thermostat_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_room_thermostat_item_count();
    klim_state_t state = state_snapshot();
    char right_text[16];

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "ROOM THERMOSTAT", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 34 + (int)i * 24;
        const bool selected = (i == s_ui.selected_room_thermostat_index);

        if (i == 0u) {
            snprintf(right_text, sizeof(right_text), "%.1fC", state.auto_hysteresis_c);
        } else {
            snprintf(right_text, sizeof(right_text), "%+.1fC", state.temp_calibration_c);
        }

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_room_thermostat_menu_items[i].title, bg);
            draw_text5x7_rot90cw(x + 95, y + 6, 1, right_text, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_room_thermostat_menu_items[i].title, fg);
            draw_text5x7_rot90cw(x + 95, y + 6, 1, right_text, fg);
        }
    }
}

static void draw_io_config_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_io_config_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "I/O CONFIG", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 28 + (int)i * 24;
        const bool selected = (i == s_ui.selected_io_config_index);

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_io_config_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_io_config_menu_items[i].title, fg);
        }
    }
}

static void draw_room_thermostat_hysteresis_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    klim_state_t state = state_snapshot();
    char value_buf[16];

    snprintf(value_buf, sizeof(value_buf), "%.1fC", state.auto_hysteresis_c);

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "HYSTERESIS", bg);

    lcd_draw_rect_rot90cw(14, 30, 132, 44, 1, fg);
    draw_centered_text5x7_rot90cw(20, 36, 120, 10, 1, "CURRENT VALUE", fg);
    draw_centered_text5x7_rot90cw(20, 50, 120, 16, 2, value_buf, fg);

    lcd_draw_rect_rot90cw(14, 82, 132, 34, 1, fg);
    draw_centered_text5x7_rot90cw(18, 88, 124, 10, 1, "RANGE 0.0 TO 10.0C", fg);
    draw_centered_text5x7_rot90cw(18, 100, 124, 10, 1, "STEP 0.5C", fg);
}

static void draw_room_thermostat_calibration_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    klim_state_t state = state_snapshot();
    char value_buf[16];

    snprintf(value_buf, sizeof(value_buf), "%+.1fC", state.temp_calibration_c);

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "CALIBRATION", bg);

    lcd_draw_rect_rot90cw(14, 30, 132, 44, 1, fg);
    draw_centered_text5x7_rot90cw(20, 36, 120, 10, 1, "CURRENT VALUE", fg);
    draw_centered_text5x7_rot90cw(20, 50, 120, 16, 2, value_buf, fg);

    lcd_draw_rect_rot90cw(14, 82, 132, 34, 1, fg);
    draw_centered_text5x7_rot90cw(18, 88, 124, 10, 1, "RANGE -10.0 TO +10.0C", fg);
    draw_centered_text5x7_rot90cw(18, 100, 124, 10, 1, "STEP 0.1C", fg);
}

static void draw_network_setup_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_network_setup_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "NETWORK SETUP", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 8;
        const int y = 26 + (int)i * 12;
        const bool selected = (i == s_ui.selected_network_setup_index);

        lcd_draw_rect_rot90cw(x, y, 144, 10, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 142, 8, fg);
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_network_setup_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_network_setup_menu_items[i].title, fg);
        }
    }
}

static void draw_wifi_setup_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_wifi_setup_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "WIFI SETUP", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 8;
        const int y = 26 + (int)i * 12;
        const bool selected = (i == s_ui.selected_wifi_setup_index);

        lcd_draw_rect_rot90cw(x, y, 144, 10, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 142, 8, fg);
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_wifi_setup_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 4, y + 2, 1, s_wifi_setup_menu_items[i].title, fg);
        }
    }
}

static void draw_service_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_service_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "SERVICE MENU", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 34 + (int)i * 24;
        const bool selected = (i == s_ui.selected_service_index);

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_service_menu_items[i].title, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_service_menu_items[i].title, fg);
        }
    }
}

static void draw_zones_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_zone_item_count();

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "ZONES", bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 10;
        const int y = 26 + (int)i * 12;
        const bool selected = (i == s_ui.selected_zone_index);
        char line[12];

        snprintf(line, sizeof(line), "ZONE %u", (unsigned)(i + 1u));
        lcd_draw_rect_rot90cw(x, y, 140, 10, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 8, fg);
            draw_text5x7_rot90cw(x + 5, y + 2, 1, line, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 2, 1, line, fg);
        }
    }
}

static const char *zone_temp_source_to_string(zone_temp_source_t source)
{
    return (source == ZONE_TEMP_SOURCE_CHRONOGRAM) ? "CHRONOGRAM" : "THERMOSTAT";
}

static void draw_zone_setpoint_label_rot90cw(int x, int y, int w, int h, uint16_t color)
{
    draw_centered_text5x7_rot90cw(x, y, w, h, 1, "SETPOINT", color);
}

static void draw_temp_c_value_rot90cw(int x, int y, int scale, uint8_t temp_c, uint16_t color)
{
    char value[8];

    snprintf(value, sizeof(value), "%u", (unsigned)temp_c);
    draw_temp_string_rot90cw(x, y, scale, value, color);
    draw_char5x7_rot90cw(x + text_width5x7(value, scale) + scale * 3, y, scale, 'C', color);
}

static void draw_zone_detail_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const size_t item_count = ui_zone_detail_item_count();
    const zone_state_t *zone = ui_get_selected_zone_state();
    char zone_title[16];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, zone_title, bg);

    for (size_t i = 0; i < item_count; ++i) {
        const int x = 8;
        const int y = 28 + (int)i * 24;
        const bool selected = (i == s_ui.selected_zone_detail_index);
        const char *right_text = "";

        if (s_zone_detail_menu_items[i].id == ZONE_DETAIL_CONNECTION) {
            right_text = zone->paired ? "SYNCED" : "OPEN";
        } else if (s_zone_detail_menu_items[i].id == ZONE_DETAIL_TEMP_SOURCE) {
            right_text = zone_temp_source_to_string(zone->temp_source);
        } else if (s_zone_detail_menu_items[i].id == ZONE_DETAIL_CHRONOGRAM) {
            right_text = "3 ITEMS";
        }

        lcd_draw_rect_rot90cw(x, y, 144, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 142, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_detail_menu_items[i].title, bg);
            draw_text5x7_rot90cw(x + 82, y + 6, 1, right_text, bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_detail_menu_items[i].title, fg);
            draw_text5x7_rot90cw(x + 82, y + 6, 1, right_text, fg);
        }
    }
}

static void draw_zone_connection_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const zone_state_t *zone = ui_get_selected_zone_state();
    char zone_title[16];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u LINK", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, zone_title, bg);
    draw_text5x7_rot90cw(10, 28, 1, zone->paired ? "STATUS: SYNCED" : "STATUS: OPEN", fg);

    for (size_t i = 0; i < ui_zone_connection_item_count(); ++i) {
        const int x = 10;
        const int y = 44 + (int)i * 22;
        const bool selected = (i == s_ui.selected_zone_connection_index);

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_connection_menu_items[i], bg);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_connection_menu_items[i], fg);
        }
    }
}

static void draw_zone_sync_confirm_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    char zone_title[16];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u SYNC", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(10, 18, 140, 92, 2, fg);
    lcd_fill_rect_rot90cw(10, 18, 140, 18, fg);
    draw_centered_text5x7_rot90cw(10, 18, 140, 18, 2, zone_title, bg);
    draw_centered_text5x7_rot90cw(18, 48, 124, 14, 2, "CONNECT?", fg);

    for (size_t i = 0; i < 2u; ++i) {
        const int x = 24 + (int)i * 60;
        const int y = 78;
        const bool selected = (i == s_ui.selected_zone_sync_confirm_index);
        const char *label = (i == 0u) ? "YES" : "NO";

        lcd_draw_rect_rot90cw(x, y, 52, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 50, 16, fg);
            draw_centered_text5x7_rot90cw(x, y, 52, 18, 2, label, bg);
        } else {
            draw_centered_text5x7_rot90cw(x, y, 52, 18, 2, label, fg);
        }
    }
}

static void draw_zone_sync_wait_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const int64_t now_ms = ui_now_ms();
    const int phase = (int)(now_ms % ZONE_SYNC_ANIM_PERIOD_MS);
    const int half = ZONE_SYNC_ANIM_PERIOD_MS / 2;
    const int radius = 2 + ((phase <= half) ?
                       (phase * 39 / half) :
                       ((ZONE_SYNC_ANIM_PERIOD_MS - phase) * 39 / half));
    char zone_title[16];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u SYNC", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, zone_title, bg);
    draw_centered_text5x7_rot90cw(24, 28, 112, 12, 1, "WAITING FOR TX", fg);
    draw_circle_ring_rot90cw(80, 82, radius, 2, fg);
}

static void draw_zone_temp_source_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const zone_state_t *zone = ui_get_selected_zone_state();
    char zone_title[16];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u TEMP", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, zone_title, bg);

    for (size_t i = 0; i < ui_zone_temp_source_item_count(); ++i) {
        const int x = 10;
        const int y = 34 + (int)i * 24;
        const bool selected = (i == s_ui.selected_zone_temp_source_index);
        const bool active = ((i == 0u) && (zone->temp_source == ZONE_TEMP_SOURCE_THERMOSTAT)) ||
                            ((i == 1u) && (zone->temp_source == ZONE_TEMP_SOURCE_CHRONOGRAM));

        lcd_draw_rect_rot90cw(x, y, 140, 18, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 138, 16, fg);
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_temp_source_menu_items[i], bg);
            draw_status_square_rot90cw(132, y + 5, active);
        } else {
            draw_text5x7_rot90cw(x + 5, y + 6, 1, s_zone_temp_source_menu_items[i], fg);
            draw_status_square_rot90cw(132, y + 5, active);
        }
    }
}

static void draw_zone_chronogram_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    const zone_state_t *zone = ui_get_selected_zone_state();
    char zone_title[20];

    snprintf(zone_title, sizeof(zone_title), "ZONE %u CHRONO", (unsigned)ui_get_selected_zone_number());

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(2, 2, 156, 124, 2, fg);
    lcd_fill_rect_rot90cw(2, 2, 156, 18, fg);
    draw_centered_text5x7_rot90cw(2, 2, 156, 18, 2, zone_title, bg);

    for (size_t i = 0; i < 3u; ++i) {
        const zone_schedule_entry_t *entry = &zone->schedule[i];
        const int x = 6;
        const int y = 24 + (int)i * 18;
        const bool selected = (i == s_ui.selected_chronogram_index);
        char time_line[32];
        char temp_line[12];
        char idx[4];

        snprintf(idx, sizeof(idx), "%u.", (unsigned)(i + 1u));
        snprintf(time_line, sizeof(time_line), "%02u:%02u-%02u:%02u",
                 (unsigned)entry->start_hour, (unsigned)entry->start_minute,
                 (unsigned)entry->end_hour, (unsigned)entry->end_minute);
        snprintf(temp_line, sizeof(temp_line), "T:%uC", (unsigned)entry->target_temp_c);

        if (selected) {
            lcd_draw_rect_rot90cw(x, y, 148, 16, 1, fg);
            lcd_fill_rect_rot90cw(x + 1, y + 1, 146, 14, fg);
            draw_text5x7_rot90cw(x + 4, y + 4, 1, idx, bg);
            draw_text5x7_rot90cw(x + 18, y + 4, 1, time_line, bg);
            draw_text5x7_rot90cw(x + 108, y + 4, 1, temp_line, bg);
        } else {
            lcd_draw_rect_rot90cw(x, y, 148, 16, 1, fg);
            draw_text5x7_rot90cw(x + 4, y + 4, 1, idx, fg);
            draw_text5x7_rot90cw(x + 18, y + 4, 1, time_line, fg);
            draw_text5x7_rot90cw(x + 108, y + 4, 1, temp_line, fg);
        }
    }

    lcd_draw_rect_rot90cw(6, 82, 148, 40, 1, fg);
    draw_zone_setpoint_label_rot90cw(10, 86, 140, 10, fg);
    draw_temp_c_value_rot90cw(42, 98, 3, zone->base_set_temp_c, fg);
}

static void draw_placeholder_screen(const char *title)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(6, 6, 148, 116, 2, fg);
    draw_centered_text5x7_rot90cw(6, 16, 148, 18, 2, title, fg);
    draw_centered_text5x7_rot90cw(6, 50, 148, 12, 1, "MENU STRUCTURE", fg);
    draw_centered_text5x7_rot90cw(6, 68, 148, 12, 1, "READY FOR CONTENT", fg);
}

static void draw_outdoor_sensor_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "OUTDOOR SENSOR", bg);

    lcd_draw_rect_rot90cw(10, 30, 140, 72, 1, fg);
    draw_centered_text5x7_rot90cw(14, 38, 132, 10, 1, "ENABLES INPUT FOR", fg);
    draw_centered_text5x7_rot90cw(14, 52, 132, 10, 1, "OUTDOOR TEMP", fg);
    draw_centered_text5x7_rot90cw(14, 66, 132, 10, 1, "NTC 10K SENSOR", fg);
    draw_centered_text5x7_rot90cw(14, 84, 132, 10, 1, "THERMISTOR", fg);
}

static void draw_delay_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;
    char value_buf[16];
    char hint_buf[32];

    lcd_fill_screen(bg);
    lcd_draw_rect_rot90cw(4, 4, 152, 120, 2, fg);
    lcd_fill_rect_rot90cw(4, 4, 152, 18, fg);
    draw_centered_text5x7_rot90cw(4, 4, 152, 18, 2, "DELAY", bg);

    for (size_t i = 0; i < ui_delay_item_count(); ++i) {
        const int x = 8;
        const int y = 28 + (int)i * 28;
        const bool selected = (i == s_ui.selected_delay_index);
        const bool editing = selected && s_ui.delay_edit_active;
        uint16_t value_s = (i == 0u) ? s_delay_settings.ac_start_delay_s : s_delay_settings.aux_relay_delay_s;

        snprintf(value_buf, sizeof(value_buf), "%us", (unsigned)value_s);

        lcd_draw_rect_rot90cw(x, y, 144, 24, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 142, 22, fg);
            draw_text5x7_rot90cw(x + 4, y + 4, 1, s_delay_item_titles[i], bg);
            draw_text5x7_rot90cw(x + 4, y + 14, 1, value_buf, bg);
            if (editing) {
                draw_text5x7_rot90cw(x + 104, y + 14, 1, "EDIT", bg);
            }
        } else {
            draw_text5x7_rot90cw(x + 4, y + 4, 1, s_delay_item_titles[i], fg);
            draw_text5x7_rot90cw(x + 4, y + 14, 1, value_buf, fg);
        }
    }

    lcd_draw_rect_rot90cw(8, 86, 144, 34, 1, fg);
    snprintf(hint_buf, sizeof(hint_buf), "A/C D:%us R:%u-%us",
             (unsigned)AC_START_DELAY_DEFAULT_S,
             (unsigned)AC_START_DELAY_MIN_S,
             (unsigned)AC_START_DELAY_MAX_S);
    draw_text5x7_rot90cw(12, 94, 1, hint_buf, fg);
    snprintf(hint_buf, sizeof(hint_buf), "AUX D:%us R:%u-%us",
             (unsigned)AUX_RELAY_DELAY_DEFAULT_S,
             (unsigned)AUX_RELAY_DELAY_MIN_S,
             (unsigned)AUX_RELAY_DELAY_MAX_S);
    draw_text5x7_rot90cw(12, 106, 1, hint_buf, fg);
}

static void draw_factory_reset_screen(void)
{
    const uint16_t bg = 0xFFFF;
    const uint16_t fg = 0x0000;

    draw_installer_screen();

    lcd_fill_rect_rot90cw(18, 28, 124, 76, bg);
    lcd_draw_rect_rot90cw(18, 28, 124, 76, 2, fg);
    lcd_fill_rect_rot90cw(18, 28, 124, 14, fg);
    draw_centered_text5x7_rot90cw(18, 28, 124, 14, 1, "FACTORY RESET", bg);
    draw_centered_text5x7_rot90cw(24, 48, 112, 10, 1, "DO YOU REALLY WANT", fg);
    draw_centered_text5x7_rot90cw(24, 60, 112, 10, 1, "TO RESET", fg);
    draw_centered_text5x7_rot90cw(24, 72, 112, 10, 1, "SETTINGS?", fg);

    for (size_t i = 0; i < ui_factory_reset_item_count(); ++i) {
        const int x = 28 + (int)i * 48;
        const int y = 88;
        const bool selected = (i == s_ui.selected_factory_reset_index);

        lcd_draw_rect_rot90cw(x, y, 38, 12, 1, fg);
        if (selected) {
            lcd_fill_rect_rot90cw(x + 1, y + 1, 36, 10, fg);
            draw_centered_text5x7_rot90cw(x + 1, y + 1, 36, 10, 1, s_factory_reset_items[i], bg);
        } else {
            draw_centered_text5x7_rot90cw(x + 1, y + 1, 36, 10, 1, s_factory_reset_items[i], fg);
        }
    }
}

static void draw_current_screen(void)
{
    if (s_ui_mutex != NULL) {
        xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    }

    const klim_state_t state = state_snapshot();

    switch (s_ui.current_screen) {
    case UI_SCREEN_HOME:
        draw_dashboard_screen(&state);
        break;

    case UI_SCREEN_WORK_MODE:
        draw_mode_screen(&state);
        break;

    case UI_SCREEN_SYSTEM:
        draw_system_screen();
        break;

    case UI_SCREEN_INSTALLER:
        draw_installer_screen();
        break;

    case UI_SCREEN_INSTALLER_ROOM_THERMOSTAT:
        draw_room_thermostat_screen();
        break;

    case UI_SCREEN_INSTALLER_IO_CONFIG:
        draw_io_config_screen();
        break;

    case UI_SCREEN_INSTALLER_NETWORK_SETUP:
        draw_network_setup_screen();
        break;

    case UI_SCREEN_INSTALLER_WIFI_SETUP:
        draw_wifi_setup_screen();
        break;

    case UI_SCREEN_SERVICE:
        draw_service_screen();
        break;

    case UI_SCREEN_ZONES:
        draw_zones_screen();
        break;

    case UI_SCREEN_ZONE_DETAIL:
        draw_zone_detail_screen();
        break;

    case UI_SCREEN_ZONE_CONNECTION:
        draw_zone_connection_screen();
        break;

    case UI_SCREEN_ZONE_SYNC_CONFIRM:
        draw_zone_sync_confirm_screen();
        break;

    case UI_SCREEN_ZONE_SYNC_WAIT:
        draw_zone_sync_wait_screen();
        break;

    case UI_SCREEN_ZONE_TEMP_SOURCE:
        draw_zone_temp_source_screen();
        break;

    case UI_SCREEN_ZONE_CHRONOGRAM:
        draw_zone_chronogram_screen();
        break;

    case UI_SCREEN_SYSTEM_TIME:
        draw_system_time_menu_screen();
        break;

    case UI_SCREEN_SYSTEM_DATE_SETUP:
        draw_system_date_setup_screen();
        break;

    case UI_SCREEN_SYSTEM_TIME_SETUP:
        draw_system_time_setup_screen();
        break;

    case UI_SCREEN_SYSTEM_DISPLAY:
        draw_placeholder_screen("DISPLAY SETUP");
        break;

    case UI_SCREEN_SYSTEM_LANGUAGE:
        draw_placeholder_screen("SYSTEM LANGUAGE");
        break;

    case UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_HYSTERESIS:
        draw_room_thermostat_hysteresis_screen();
        break;

    case UI_SCREEN_INSTALLER_ROOM_THERMOSTAT_CALIBRATION:
        draw_room_thermostat_calibration_screen();
        break;

    case UI_SCREEN_INSTALLER_IO_ZONE_RELAYS:
        draw_placeholder_screen("ZONE RELAYS");
        break;

    case UI_SCREEN_INSTALLER_IO_DRY_CONTACT_RELAY:
        draw_placeholder_screen("DRY CONTACT RELAY");
        break;

    case UI_SCREEN_INSTALLER_IO_ZONE_AUX_SENSORS:
        draw_placeholder_screen("ZONE AUX SENSORS");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_DHCP:
        draw_placeholder_screen("DHCP");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_IP_ADDRESS:
        draw_placeholder_screen("IP ADDRESS");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_SUBNET_MASK:
        draw_placeholder_screen("SUBNET MASK");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_GATEWAY:
        draw_placeholder_screen("GATEWAY");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_DNS:
        draw_placeholder_screen("DNS");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_HOSTNAME:
        draw_placeholder_screen("HOSTNAME");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_LINK_STATUS:
        draw_placeholder_screen("LINK STATUS");
        break;

    case UI_SCREEN_INSTALLER_NETWORK_MAC_ADDRESS:
        draw_placeholder_screen("MAC ADDRESS");
        break;

    case UI_SCREEN_INSTALLER_WIFI_ENABLE:
        draw_placeholder_screen("WIFI ENABLE");
        break;

    case UI_SCREEN_INSTALLER_WIFI_SSID:
        draw_placeholder_screen("SSID");
        break;

    case UI_SCREEN_INSTALLER_WIFI_PASSWORD:
        draw_placeholder_screen("PASSWORD");
        break;

    case UI_SCREEN_INSTALLER_WIFI_DHCP:
        draw_placeholder_screen("DHCP");
        break;

    case UI_SCREEN_INSTALLER_WIFI_IP_ADDRESS:
        draw_placeholder_screen("IP ADDRESS");
        break;

    case UI_SCREEN_INSTALLER_WIFI_GATEWAY:
        draw_placeholder_screen("GATEWAY");
        break;

    case UI_SCREEN_INSTALLER_WIFI_STATUS:
        draw_placeholder_screen("STATUS");
        break;

    case UI_SCREEN_INSTALLER_WIFI_SIGNAL:
        draw_placeholder_screen("SIGNAL");
        break;

    case UI_SCREEN_INSTALLER_MANUAL_CONTROL:
        draw_placeholder_screen("MANUAL CONTROL");
        break;

    case UI_SCREEN_INSTALLER_OUTDOOR_SENSOR:
        draw_outdoor_sensor_screen();
        break;

    case UI_SCREEN_INSTALLER_DELAY:
        draw_delay_screen();
        break;

    case UI_SCREEN_INSTALLER_FACTORY_RESET:
        draw_factory_reset_screen();
        break;

    case UI_SCREEN_BOOT:
    default:
        draw_splash_screen();
        break;
    }

    lcd_present();

    if (s_ui_mutex != NULL) {
        xSemaphoreGive(s_ui_mutex);
    }
}

static float clamp_target_temp(float value)
{
    if (value < HVAC_MIN_TEMP_C) {
        return HVAC_MIN_TEMP_C;
    }
    if (value > HVAC_MAX_TEMP_C) {
        return HVAC_MAX_TEMP_C;
    }
    return value;
}

static const char *mode_to_string(hvac_mode_t mode)
{
    switch (mode) {
    case HVAC_MODE_COOL:
        return "cool";
    case HVAC_MODE_HEAT:
        return "heat";
    case HVAC_MODE_FAN_ONLY:
        return "fan_only";
    case HVAC_MODE_AUTO:
        return "auto";
    case HVAC_MODE_OFF:
    default:
        return "off";
    }
}

static const char *mode_to_ui_label(hvac_mode_t mode)
{
    if (mode == HVAC_MODE_OFF) {
        return "OFF";
    }
    for (size_t i = 0; i < ui_mode_item_count(); ++i) {
        if (s_mode_menu_items[i].mode == mode) {
            return s_mode_menu_items[i].title;
        }
    }
    return "COOL";
}

static bool mode_from_string(const char *value, hvac_mode_t *mode)
{
    if (strcasecmp(value, "off") == 0) {
        *mode = HVAC_MODE_OFF;
    } else if (strcasecmp(value, "cool") == 0) {
        *mode = HVAC_MODE_COOL;
    } else if (strcasecmp(value, "heat") == 0) {
        *mode = HVAC_MODE_HEAT;
    } else if ((strcasecmp(value, "fan_only") == 0) || (strcasecmp(value, "fan") == 0)) {
        *mode = HVAC_MODE_FAN_ONLY;
    } else if (strcasecmp(value, "auto") == 0) {
        *mode = HVAC_MODE_AUTO;
    } else {
        return false;
    }
    return true;
}

static const char *action_from_state(const klim_state_t *state)
{
    const float diff = state->current_temp_c - state->target_temp_c;

    if (state->mode == HVAC_MODE_OFF) {
        return "off";
    }
    if (!state->relay_on) {
        return "idle";
    }
    switch (state->mode) {
    case HVAC_MODE_COOL:
        return "cooling";
    case HVAC_MODE_HEAT:
        return "heating";
    case HVAC_MODE_FAN_ONLY:
        return "fan";
    case HVAC_MODE_AUTO:
        if (diff >= state->auto_hysteresis_c) {
            return "cooling";
        }
        if (diff <= -state->auto_hysteresis_c) {
            return "heating";
        }
        return "idle";
    case HVAC_MODE_OFF:
    default:
        return "off";
    }
}

static void state_lock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    }
}

static void state_unlock(void)
{
    if (s_state_mutex != NULL) {
        xSemaphoreGive(s_state_mutex);
    }
}

static void zones_init_defaults(void)
{
    for (size_t i = 0; i < ZONE_MAX_COUNT; ++i) {
        s_zones[i].paired = false;
        s_zones[i].temp_source = ZONE_TEMP_SOURCE_THERMOSTAT;
        s_zones[i].base_set_temp_c = 24u;
        s_zones[i].link_up = false;
        s_zones[i].tx_id = 0u;
        s_zones[i].rssi_percent = 0u;
        s_zones[i].current_temp_c = 0.0f;
        s_zones[i].thermostat_target_temp_c = 0.0f;
        s_zones[i].target_temp_c = 0.0f;
        s_zones[i].mode = HVAC_MODE_COOL;
        s_zones[i].output_on = false;

        for (size_t j = 0; j < 3u; ++j) {
            s_zones[i].schedule[j].start_hour = 0u;
            s_zones[i].schedule[j].start_minute = 0u;
            s_zones[i].schedule[j].end_hour = 23u;
            s_zones[i].schedule[j].end_minute = 59u;
            s_zones[i].schedule[j].target_temp_c = 24u;
        }
    }
}

static void zones_sync_live_state_from_controller(void)
{
    const klim_state_t state = state_snapshot();
    const size_t count = ui_zone_item_count();

    for (size_t i = 0; i < count; ++i) {
        if (s_zones[i].paired) {
            s_zones[i].mode = state.mode;
            s_zones[i].output_on = state.relay_on;
        } else {
            s_zones[i].output_on = false;
        }
        zone_refresh_effective_target(&s_zones[i]);
    }

    ui_clamp_dashboard_zone_scroll();
}

static klim_state_t state_snapshot(void)
{
    klim_state_t copy;
    state_lock();
    copy = s_state;
    state_unlock();
    return copy;
}

static void set_wifi_connected(bool connected)
{
    state_lock();
    s_state.wifi_connected = connected;
    state_unlock();
}

static void set_ethernet_connected(bool connected)
{
    state_lock();
    s_state.ethernet_connected = connected;
    state_unlock();
}

static void set_mqtt_connected(bool connected)
{
    state_lock();
    s_state.mqtt_connected = connected;
    state_unlock();
}

static bool network_is_connected(void)
{
    bool connected;
    state_lock();
    connected = s_state.wifi_connected || s_state.ethernet_connected;
    state_unlock();
    return connected;
}

static void make_topic(char *buf, size_t len, const char *suffix)
{
    snprintf(buf, len, "%s/%s", CONFIG_AJAX_NODE_ID, suffix);
}

static void make_discovery_topic(char *buf, size_t len, const char *component, const char *object_id)
{
    snprintf(buf, len, "%s/%s/%s/config", CONFIG_AJAX_HA_DISCOVERY_PREFIX, component, object_id);
}

static bool mqtt_is_connected(void)
{
    bool connected;
    state_lock();
    connected = s_state.mqtt_connected;
    state_unlock();
    return connected;
}

static void mqtt_publish_text(const char *topic, const char *payload, bool retained)
{
    if ((s_mqtt_client == NULL) || !mqtt_is_connected()) {
        return;
    }
    esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, retained ? 1 : 0);
}

static void mqtt_publish_state(void)
{
    char topic[128];
    char payload[64];
    klim_state_t state = state_snapshot();

    make_topic(topic, sizeof(topic), "availability");
    mqtt_publish_text(topic, (state.wifi_connected || state.ethernet_connected) ? "online" : "offline", true);

    make_topic(topic, sizeof(topic), "state/mode");
    mqtt_publish_text(topic, mode_to_string(state.mode), true);

    make_topic(topic, sizeof(topic), "state/action");
    mqtt_publish_text(topic, action_from_state(&state), true);

    snprintf(payload, sizeof(payload), "%.1f", state.current_temp_c);
    make_topic(topic, sizeof(topic), "state/current_temp");
    mqtt_publish_text(topic, payload, true);

    snprintf(payload, sizeof(payload), "%.1f", state.target_temp_c);
    make_topic(topic, sizeof(topic), "state/target_temp");
    mqtt_publish_text(topic, payload, true);

    make_topic(topic, sizeof(topic), "state/relay");
    mqtt_publish_text(topic, state.relay_on ? "ON" : "OFF", true);

    make_topic(topic, sizeof(topic), "state/rx_link");
    mqtt_publish_text(topic, state.rx_link ? "ON" : "OFF", true);
}

static void mqtt_publish_discovery(void)
{
    char topic[192];
    char payload[2048];
    char avail_topic[160];
    char mode_cmd_topic[160];
    char mode_state_topic[160];
    char temp_cmd_topic[160];
    char temp_state_topic[160];
    char current_temp_topic[160];
    char action_topic[160];
    char relay_topic[160];
    char link_topic[160];

    make_topic(avail_topic, sizeof(avail_topic), "availability");
    make_topic(mode_cmd_topic, sizeof(mode_cmd_topic), "cmd/mode");
    make_topic(mode_state_topic, sizeof(mode_state_topic), "state/mode");
    make_topic(temp_cmd_topic, sizeof(temp_cmd_topic), "cmd/target_temp");
    make_topic(temp_state_topic, sizeof(temp_state_topic), "state/target_temp");
    make_topic(current_temp_topic, sizeof(current_temp_topic), "state/current_temp");
    make_topic(action_topic, sizeof(action_topic), "state/action");
    make_topic(relay_topic, sizeof(relay_topic), "state/relay");
    make_topic(link_topic, sizeof(link_topic), "state/rx_link");

    make_discovery_topic(topic, sizeof(topic), "climate", CONFIG_AJAX_NODE_ID "_climate");
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"unique_id\":\"%s_climate\","
             "\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
             "\"mode_command_topic\":\"%s\",\"mode_state_topic\":\"%s\","
             "\"temperature_command_topic\":\"%s\",\"temperature_state_topic\":\"%s\","
             "\"current_temperature_topic\":\"%s\",\"action_topic\":\"%s\","
             "\"temperature_unit\":\"C\",\"min_temp\":16,\"max_temp\":30,\"temp_step\":1,"
             "\"modes\":[\"off\",\"cool\",\"heat\",\"fan_only\",\"auto\"],"
             "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"AJAX KLIM\",\"model\":\"ESP32 AC Controller\",\"sw_version\":\"0.1.0\"}}",
             CONFIG_AJAX_DEVICE_NAME,
             CONFIG_AJAX_NODE_ID,
             avail_topic,
             mode_cmd_topic,
             mode_state_topic,
             temp_cmd_topic,
             temp_state_topic,
             current_temp_topic,
             action_topic,
             CONFIG_AJAX_NODE_ID,
             CONFIG_AJAX_DEVICE_NAME);
    mqtt_publish_text(topic, payload, true);

    make_discovery_topic(topic, sizeof(topic), "binary_sensor", CONFIG_AJAX_NODE_ID "_relay");
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s Relay\",\"unique_id\":\"%s_relay\","
             "\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
             "\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\"}}",
             CONFIG_AJAX_DEVICE_NAME,
             CONFIG_AJAX_NODE_ID,
             avail_topic,
             relay_topic,
             CONFIG_AJAX_NODE_ID,
             CONFIG_AJAX_DEVICE_NAME);
    mqtt_publish_text(topic, payload, true);

    make_discovery_topic(topic, sizeof(topic), "binary_sensor", CONFIG_AJAX_NODE_ID "_rx_link");
    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s RX Link\",\"unique_id\":\"%s_rx_link\","
             "\"availability_topic\":\"%s\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
             "\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
             "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\"}}",
             CONFIG_AJAX_DEVICE_NAME,
             CONFIG_AJAX_NODE_ID,
             avail_topic,
             link_topic,
             CONFIG_AJAX_NODE_ID,
             CONFIG_AJAX_DEVICE_NAME);
    mqtt_publish_text(topic, payload, true);

    s_discovery_sent = true;
}

static void uart_send_command(const klim_state_t *state)
{
#if CONFIG_AJAX_ENABLE_UART
    char line[96];
    (void)snprintf(line, sizeof(line), "MODE=%s;TARGET=%.1f\n", mode_to_string(state->mode), state->target_temp_c);
    uart_tx_lock();
    ESP_LOGI(TAG, "UART TX: %s", line);
    uart_write_text_slow(line, pdMS_TO_TICKS(10));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(250));
    uart_tx_unlock();
#else
    (void)state;
#endif
}

static void uart_send_mode_override(hvac_mode_t mode)
{
#if CONFIG_AJAX_ENABLE_UART
    char line_short[16];
    char mode_char = 'o';

    switch (mode) {
    case HVAC_MODE_COOL:
        mode_char = 'c';
        break;
    case HVAC_MODE_HEAT:
        mode_char = 'h';
        break;
    case HVAC_MODE_AUTO:
        mode_char = 'a';
        break;
    case HVAC_MODE_FAN_ONLY:
        mode_char = 'f';
        break;
    case HVAC_MODE_OFF:
    default:
        mode_char = 'o';
        break;
    }

    (void)snprintf(line_short, sizeof(line_short), "M=%c\n", mode_char);

    uart_tx_lock();
    for (uint32_t i = 0; i < 12u; ++i) {
        ESP_LOGI(TAG, "UART TX: %s", line_short);
        uart_write_text_slow(line_short, pdMS_TO_TICKS(5));
        uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(250));
        if (i < 11u) {
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }
    uart_tx_unlock();
#else
    (void)mode;
#endif
}

static void uart_send_target_override(float target_c)
{
#if CONFIG_AJAX_ENABLE_UART
    char line_short[16];

    (void)snprintf(line_short, sizeof(line_short), "T=%.0f\n", target_c);

    uart_tx_lock();
    for (uint32_t i = 0; i < 16u; ++i) {
        ESP_LOGI(TAG, "UART TX: %s", line_short);
        uart_write_text_slow(line_short, pdMS_TO_TICKS(5));
        uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(250));
        if (i < 15u) {
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }
    uart_tx_unlock();
#else
    (void)target_c;
#endif
}

static void uart_write_text_slow(const char *line, TickType_t inter_byte_delay_ticks)
{
#if CONFIG_AJAX_ENABLE_UART
    while (*line != '\0') {
        uart_write_bytes(UART_PORT_NUM, line, 1);
        ++line;
        if ((*line != '\0') && (inter_byte_delay_ticks > 0)) {
            vTaskDelay(inter_byte_delay_ticks);
        }
    }
#else
    (void)line;
    (void)inter_byte_delay_ticks;
#endif
}

static void uart_tx_lock(void)
{
#if CONFIG_AJAX_ENABLE_UART
    if (s_uart_mutex != NULL) {
        xSemaphoreTake(s_uart_mutex, portMAX_DELAY);
    }
#endif
}

static void uart_tx_unlock(void)
{
#if CONFIG_AJAX_ENABLE_UART
    if (s_uart_mutex != NULL) {
        xSemaphoreGive(s_uart_mutex);
    }
#endif
}

static void uart_send_zone_sync_request(uint8_t zone)
{
#if CONFIG_AJAX_ENABLE_UART
    char line[24];
    (void)snprintf(line, sizeof(line), "SYNC=%u\n", (unsigned)zone);
    ESP_LOGI(TAG, "UART TX: %s", line);
    uart_tx_lock();
    uart_write_text_slow(line, pdMS_TO_TICKS(35));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(250));
    uart_tx_unlock();
#else
    (void)zone;
#endif
}

static void uart_send_zone_unpair_request(uint8_t zone)
{
#if CONFIG_AJAX_ENABLE_UART
    char line[24];
    (void)snprintf(line, sizeof(line), "UNPAIR=%u\n", (unsigned)zone);
    ESP_LOGI(TAG, "UART TX: %s", line);
    uart_tx_lock();
    uart_write_text_slow(line, pdMS_TO_TICKS(35));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(250));
    uart_tx_unlock();
#else
    (void)zone;
#endif
}

static bool mqtt_event_topic_equals(esp_mqtt_event_handle_t event, const char *topic)
{
    size_t len = strlen(topic);
    return ((size_t)event->topic_len == len) && (strncmp(event->topic, topic, len) == 0);
}

static void mqtt_event_copy_data(esp_mqtt_event_handle_t event, char *dst, size_t dst_len)
{
    size_t copy_len = (size_t)event->data_len;
    if (copy_len >= dst_len) {
        copy_len = dst_len - 1;
    }
    memcpy(dst, event->data, copy_len);
    dst[copy_len] = '\0';
}

static void handle_mode_command(const char *value)
{
    hvac_mode_t mode;
    if (!mode_from_string(value, &mode)) {
        ESP_LOGW(TAG, "Unknown HVAC mode command: %s", value);
        return;
    }

    state_lock();
    s_state.mode = mode;
    if (mode == HVAC_MODE_OFF) {
        s_state.relay_on = false;
    }
    state_unlock();
    zones_sync_live_state_from_controller();

    uart_send_mode_override(mode);
    mqtt_publish_state();
}

static void handle_target_command(const char *value)
{
    float target = strtof(value, NULL);
    if (target <= 0.0f) {
        ESP_LOGW(TAG, "Invalid target temperature command: %s", value);
        return;
    }

    state_lock();
    s_state.target_temp_c = clamp_target_temp(target);
    s_target_override_pending = true;
    s_target_override_pending_c = s_state.target_temp_c;
    s_target_override_deadline_ms = ui_now_ms() + TARGET_OVERRIDE_TIMEOUT_MS;
    s_target_override_last_send_ms = 0;
    state_unlock();

    uart_send_target_override(target);
    mqtt_publish_state();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    char topic[128];
    char payload[128];
    esp_mqtt_event_handle_t event = event_data;

    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        set_mqtt_connected(true);
        make_topic(topic, sizeof(topic), "cmd/mode");
        esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
        make_topic(topic, sizeof(topic), "cmd/target_temp");
        esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
        esp_mqtt_client_subscribe(s_mqtt_client, "homeassistant/status", 1);
        mqtt_publish_discovery();
        mqtt_publish_state();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        set_mqtt_connected(false);
        s_discovery_sent = false;
        break;

    case MQTT_EVENT_DATA:
        make_topic(topic, sizeof(topic), "cmd/mode");
        if (mqtt_event_topic_equals(event, topic)) {
            mqtt_event_copy_data(event, payload, sizeof(payload));
            handle_mode_command(payload);
            break;
        }

        make_topic(topic, sizeof(topic), "cmd/target_temp");
        if (mqtt_event_topic_equals(event, topic)) {
            mqtt_event_copy_data(event, payload, sizeof(payload));
            handle_target_command(payload);
            break;
        }

        if (mqtt_event_topic_equals(event, "homeassistant/status")) {
            mqtt_event_copy_data(event, payload, sizeof(payload));
            if (strcmp(payload, "online") == 0) {
                mqtt_publish_discovery();
                mqtt_publish_state();
            }
        }
        break;

    default:
        break;
    }
}

static void start_mqtt_if_needed(void)
{
    char availability_topic[128];
    esp_mqtt_client_config_t mqtt_cfg = {0};

    if (s_mqtt_client != NULL) {
        return;
    }
    if (!network_is_connected()) {
        ESP_LOGI(TAG, "No IP connectivity yet, MQTT start postponed");
        return;
    }
    if (strlen(CONFIG_AJAX_MQTT_URI) == 0) {
        ESP_LOGW(TAG, "MQTT URI is empty, MQTT disabled");
        return;
    }

    make_topic(availability_topic, sizeof(availability_topic), "availability");

    mqtt_cfg.broker.address.uri = CONFIG_AJAX_MQTT_URI;
    mqtt_cfg.credentials.username = (strlen(CONFIG_AJAX_MQTT_USERNAME) > 0) ? CONFIG_AJAX_MQTT_USERNAME : NULL;
    mqtt_cfg.credentials.authentication.password = (strlen(CONFIG_AJAX_MQTT_PASSWORD) > 0) ? CONFIG_AJAX_MQTT_PASSWORD : NULL;
    mqtt_cfg.credentials.client_id = CONFIG_AJAX_NODE_ID;
    mqtt_cfg.session.last_will.topic = availability_topic;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.network.reconnect_timeout_ms = 5000;

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    (void)arg;
    (void)event_base;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr));
        ESP_LOGI(TAG, "Ethernet link up");
        ESP_LOGI(TAG, "Ethernet MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
        break;

    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet link down");
        set_ethernet_connected(false);
        break;

    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet started");
        break;

    case ETHERNET_EVENT_STOP:
        ESP_LOGW(TAG, "Ethernet stopped");
        set_ethernet_connected(false);
        break;

    default:
        break;
    }
}

static void eth_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;

        ESP_LOGI(TAG, "Ethernet got IP");
        ESP_LOGI(TAG, "ETH IP: " IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "ETH MASK: " IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "ETH GW: " IPSTR, IP2STR(&ip_info->gw));
        set_ethernet_connected(true);
        start_mqtt_if_needed();
    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
        ESP_LOGW(TAG, "Ethernet lost IP");
        set_ethernet_connected(false);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_START)) {
        esp_wifi_connect();
    } else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting");
        set_wifi_connected(false);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP)) {
        ESP_LOGI(TAG, "Wi-Fi connected");
        set_wifi_connected(true);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        start_mqtt_if_needed();
    }
}

static void wifi_init_sta(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};

    if (strlen(CONFIG_AJAX_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty, network disabled");
        return;
    }

    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    memcpy(wifi_config.sta.ssid, CONFIG_AJAX_WIFI_SSID, strlen(CONFIG_AJAX_WIFI_SSID));
    memcpy(wifi_config.sta.password, CONFIG_AJAX_WIFI_PASSWORD, strlen(CONFIG_AJAX_WIFI_PASSWORD));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Prefer stable current draw over modem-sleep bursts to reduce visible TFT flicker. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    /* 10 dBm is usually enough indoors and reduces RF current spikes versus max power. */
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));
}

static void ethernet_init_w5500(void)
{
#if CONFIG_AJAX_ENABLE_ETHERNET
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_AJAX_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_ETH_CS,
        .queue_size = 20,
    };
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(LCD_HOST, &spi_devcfg);
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    esp_netif_inherent_config_t eth_inherent = ESP_NETIF_INHERENT_DEFAULT_ETH();
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    uint8_t local_mac_addr[ETH_ADDR_LEN];

    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = PIN_ETH_RST;
    w5500_config.int_gpio_num = PIN_ETH_INT;
    w5500_config.poll_period_ms = CONFIG_AJAX_ETH_SPI_POLL_MS;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(base_mac_addr));
    esp_derive_local_mac(local_mac_addr, base_mac_addr);
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, local_mac_addr));

    eth_inherent.route_prio = 120;
    netif_config.base = &eth_inherent;
    s_eth_netif = esp_netif_new(&netif_config);
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &eth_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
#else
    ESP_LOGI(TAG, "W5500 init skipped (Ethernet disabled by default)");
#endif
}

static void parse_uart_line(const char *line)
{
    char work[128];
    char work_zone[128];
    char *token;
    bool changed = false;
    uint8_t zone_number = 0u;
    bool zone_sync_completed = false;

    strncpy(work, line, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    strncpy(work_zone, line, sizeof(work_zone) - 1);
    work_zone[sizeof(work_zone) - 1] = '\0';

    for (token = strtok(work_zone, ";"); token != NULL; token = strtok(NULL, ";")) {
        char *sep = strchr(token, '=');
        if (sep == NULL) {
            sep = strchr(token, ':');
        }
        if (sep == NULL) {
            continue;
        }
        *sep++ = '\0';
        if (strcasecmp(token, "ZONE") == 0) {
            int zone = atoi(sep);
            if ((zone >= 1) && (zone <= (int)ZONE_MAX_COUNT)) {
                zone_number = (uint8_t)zone;
            }
        }
    }

    state_lock();
    strncpy(s_state.last_uart_line, line, sizeof(s_state.last_uart_line) - 1);
    s_state.last_uart_line[sizeof(s_state.last_uart_line) - 1] = '\0';

    for (token = strtok(work, ";"); token != NULL; token = strtok(NULL, ";")) {
        char *sep = strchr(token, '=');
        if (sep == NULL) {
            sep = strchr(token, ':');
        }
        if (sep == NULL) {
            continue;
        }
        *sep = '\0';
        const char *key = token;
        const char *value = sep + 1;
        zone_state_t *zone = ((zone_number >= 1u) && (zone_number <= ZONE_MAX_COUNT)) ?
                             &s_zones[zone_number - 1u] : NULL;

        if ((strcasecmp(key, "TEMP") == 0) || (strcasecmp(key, "CURRENT") == 0)) {
            float parsed_temp = strtof(value, NULL);
            if (s_state.current_temp_c != parsed_temp) {
                s_state.current_temp_c = parsed_temp;
                changed = true;
            }
            if (zone != NULL) {
                if (zone->current_temp_c != parsed_temp) {
                    zone->current_temp_c = parsed_temp;
                    changed = true;
                }
            }
        } else if ((strcasecmp(key, "TARGET") == 0) || (strcasecmp(key, "SETPOINT") == 0) || (strcasecmp(key, "SET") == 0)) {
            float parsed_target = clamp_target_temp(strtof(value, NULL));
            bool accept_target = true;

            if (s_target_override_pending) {
                if ((parsed_target >= (s_target_override_pending_c - 0.05f)) &&
                    (parsed_target <= (s_target_override_pending_c + 0.05f))) {
                    s_target_override_pending = false;
                } else {
                    accept_target = false;
                }
            }

            if (accept_target && (s_state.target_temp_c != parsed_target)) {
                s_state.target_temp_c = parsed_target;
                changed = true;
            }
            if (accept_target && (zone != NULL)) {
                if (zone->thermostat_target_temp_c != parsed_target) {
                    zone->thermostat_target_temp_c = parsed_target;
                    changed = true;
                }
                if (zone_refresh_effective_target(zone)) {
                    changed = true;
                }
            }
        } else if ((strcasecmp(key, "HYST") == 0) || (strcasecmp(key, "HYSTERESIS") == 0)) {
            const float parsed = strtof(value, NULL);
            if (parsed > 0.0f) {
                float clamped = clamp_hysteresis_value(parsed);
                if (s_state.auto_hysteresis_c != clamped) {
                    s_state.auto_hysteresis_c = clamped;
                    changed = true;
                }
            }
        } else if ((strcasecmp(key, "RELAY") == 0) || (strcasecmp(key, "HEAT") == 0) || (strcasecmp(key, "OUTPUT") == 0)) {
            bool relay_on = (atoi(value) != 0);
            if (s_state.relay_on != relay_on) {
                s_state.relay_on = relay_on;
                changed = true;
            }
        } else if ((strcasecmp(key, "LINK") == 0) || (strcasecmp(key, "RX") == 0)) {
            bool rx_link = (atoi(value) != 0);
            if (s_state.rx_link != rx_link) {
                s_state.rx_link = rx_link;
                changed = true;
            }
            if (zone != NULL) {
                if (zone->link_up != rx_link) {
                    zone->link_up = rx_link;
                    changed = true;
                }
                if (!zone->link_up && (zone->tx_id == 0u)) {
                    zone->paired = false;
                }
            }
        } else if (strcasecmp(key, "MODE") == 0) {
            hvac_mode_t mode;
            if (mode_from_string(value, &mode)) {
                if (s_state.mode != mode) {
                    s_state.mode = mode;
                    changed = true;
                }
                if (zone != NULL) {
                    if (zone->mode != mode) {
                        zone->mode = mode;
                        changed = true;
                    }
                }
            }
        } else if (strcasecmp(key, "ID") == 0) {
            if (zone != NULL) {
                uint8_t new_tx_id = (uint8_t)atoi(value);
                if (new_tx_id != 0u) {
                    for (size_t i = 0; i < ZONE_MAX_COUNT; ++i) {
                        zone_state_t *other = &s_zones[i];
                        if ((other != zone) && (other->tx_id == new_tx_id)) {
                            other->paired = false;
                            other->link_up = false;
                            other->tx_id = 0u;
                            other->rssi_percent = 0u;
                            other->current_temp_c = 0.0f;
                            other->thermostat_target_temp_c = 0.0f;
                            other->target_temp_c = 0.0f;
                            other->output_on = false;
                            changed = true;
                        }
                    }
                }
                if (zone->tx_id != new_tx_id) {
                    zone->tx_id = new_tx_id;
                    changed = true;
                }
                if (zone->paired != (zone->tx_id != 0u)) {
                    zone->paired = (zone->tx_id != 0u);
                    changed = true;
                }
                if ((s_ui.current_screen == UI_SCREEN_ZONE_SYNC_WAIT) &&
                    (zone_number == ui_get_selected_zone_number()) &&
                    zone->paired) {
                    zone_sync_completed = true;
                }
            }
        } else if (strcasecmp(key, "RSSI") == 0) {
            if (zone != NULL) {
                int rssi = atoi(value);
                if (rssi < 0) {
                    rssi = 0;
                }
                if (rssi > 100) {
                    rssi = 100;
                }
                if (zone->rssi_percent != (uint8_t)rssi) {
                    zone->rssi_percent = (uint8_t)rssi;
                    changed = true;
                }
            }
        }
    }
    state_unlock();

    if (changed) {
        zones_sync_live_state_from_controller();
        ESP_LOGI(TAG, "UART update: %s", line);
        mqtt_publish_state();
        s_home_live_update_pending = true;
        if (zone_sync_completed) {
            s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
            draw_current_screen();
            return;
        }
    }
}

static void uart_rx_task(void *arg)
{
#if CONFIG_AJAX_ENABLE_UART
    uint8_t data[64];
    char line[128];
    size_t line_len = 0;

    (void)arg;

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data), pdMS_TO_TICKS(100));
        for (int i = 0; i < len; ++i) {
            char c = (char)data[i];
            if ((c == '\r') || (c == '\n')) {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    ESP_LOGI(TAG, "UART raw: %s", line);
                    parse_uart_line(line);
                    line_len = 0;
                }
            } else if (line_len < (sizeof(line) - 1)) {
                line[line_len++] = c;
            }
        }
    }
#else
    (void)arg;
    vTaskDelete(NULL);
#endif
}

static void uart_init_bridge(void)
{
#if CONFIG_AJAX_ENABLE_UART
    uart_config_t uart_cfg = {
        .baud_rate = CONFIG_AJAX_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_RX_BUFFER_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, PIN_UART_TX, PIN_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(uart_rx_task, "uart_rx_task", UART_TASK_STACK_SIZE, NULL, 5, NULL);
#endif
}

static void state_publish_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(STATE_PUBLISH_PERIOD_MS));
        mqtt_publish_state();
    }
}

void app_main(void)
{
    esp_err_t ret;
    char last_home_date[16] = "";
    char last_home_time[8] = "";
    int64_t last_sync_anim_ms = 0;
    int64_t last_sync_uart_ms = 0;

    ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_state_mutex = xSemaphoreCreateMutex();
    s_ui_mutex = xSemaphoreCreateMutex();
    s_uart_mutex = xSemaphoreCreateMutex();
    s_wifi_event_group = xEventGroupCreate();
    zones_init_defaults();
    ds1302_init_pins();
    clock_init_factory_default();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_TFT_DC) | (1ULL << PIN_TFT_RST) | (1ULL << PIN_TFT_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    buttons_init();

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_TFT_SCK,
        .mosi_io_num = PIN_TFT_MOSI,
        .miso_io_num = PIN_ETH_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 16 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_SPI_HZ,
        .mode = 0,
        .spics_io_num = PIN_TFT_CS,
        .queue_size = 4,
        .pre_cb = lcd_spi_pre_transfer_cb,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &s_lcd));

    gpio_set_level(PIN_TFT_BL, 0);
    lcd_init_panel();
    gpio_set_level(PIN_TFT_BL, 1);
    ui_set_screen(UI_SCREEN_BOOT);
    run_boot_sequence();
    ui_set_screen(UI_SCREEN_HOME);
    draw_current_screen();
    ESP_LOGI(TAG, "Display ready");

    uart_init_bridge();
    ethernet_init_w5500();
    wifi_init_sta();
    xTaskCreate(state_publish_task, "state_publish_task", 4096, NULL, 4, NULL);
    xTaskCreate(ui_button_task, "ui_button_task", 3072, NULL, 5, NULL);

    while (1) {
        char date_text[16];
        char time_text[8];
        const int64_t now_ms = ui_now_ms();

        state_lock();
        if (s_target_override_pending) {
            if (now_ms >= s_target_override_deadline_ms) {
                s_target_override_pending = false;
            } else if ((now_ms - s_target_override_last_send_ms) >= TARGET_OVERRIDE_RETRY_MS) {
                const float pending_target = s_target_override_pending_c;
                s_target_override_last_send_ms = now_ms;
                state_unlock();
                uart_send_target_override(pending_target);
                vTaskDelay(pdMS_TO_TICKS(10));
                state_lock();
            }
        }
        state_unlock();

        clock_get_display_strings(date_text, sizeof(date_text), time_text, sizeof(time_text));
        if ((s_ui.current_screen == UI_SCREEN_HOME) &&
            !s_ui.root_menu_active &&
            (((strcmp(last_home_date, date_text) != 0) || (strcmp(last_home_time, time_text) != 0)) ||
             s_home_live_update_pending)) {
            strncpy(last_home_date, date_text, sizeof(last_home_date) - 1);
            last_home_date[sizeof(last_home_date) - 1] = '\0';
            strncpy(last_home_time, time_text, sizeof(last_home_time) - 1);
            last_home_time[sizeof(last_home_time) - 1] = '\0';
            s_home_live_update_pending = false;
            draw_current_screen();
        } else if (s_ui.current_screen == UI_SCREEN_ZONE_SYNC_WAIT) {
            if (now_ms >= s_ui.zone_sync_wait_deadline_ms) {
                uart_send_zone_unpair_request(ui_get_selected_zone_number());
                s_ui.current_screen = UI_SCREEN_ZONE_CONNECTION;
                draw_current_screen();
            } else if ((now_ms - last_sync_uart_ms) >= ZONE_SYNC_RETRY_MS) {
                last_sync_uart_ms = now_ms;
                uart_send_zone_sync_request(ui_get_selected_zone_number());
            } else if ((now_ms - last_sync_anim_ms) >= 80) {
                last_sync_anim_ms = now_ms;
                draw_current_screen();
            }
        }
        vTaskDelay(pdMS_TO_TICKS((s_ui.current_screen == UI_SCREEN_ZONE_SYNC_WAIT) ? 40 :
                                 (s_home_live_update_pending ? 50 : 200)));
    }
}

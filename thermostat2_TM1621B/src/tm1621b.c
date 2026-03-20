#include "tm1621b.h"

#include "common.h"

/* TM1621B is register-compatible with HT1621-class 32x4 LCD drivers.
 * We use the 3-wire write-only serial interface:
 *   /CS  -> PIN_TM1621_CS
 *   /WR  -> PIN_TM1621_WR
 *   DATA -> PIN_TM1621_DATA
 *
 * Hardware assumptions for this project:
 *   - /RD is tied high on the PCB
 *   - internal RC clock is selected in software
 *   - 1/3 bias, 4 commons
 */

#define TM1621B_CMD_PREFIX 0x8u /* 4 bits: 1000 */
#define TM1621B_WR_PREFIX  0x5u /* 3 bits: 101  */

#define TM1621B_CMD_SYS_DIS        0x00u
#define TM1621B_CMD_SYS_EN         0x02u
#define TM1621B_CMD_LCD_OFF        0x04u
#define TM1621B_CMD_LCD_ON         0x06u
#define TM1621B_CMD_RC_256K        0x30u
#define TM1621B_CMD_BIAS_1_3_4_COM 0x52u

static uint8_t g_tm1621b_ram[TM1621B_SEG_COUNT];

static void tm1621b_delay(void)
{
    volatile uint32_t i;

    for (i = 0u; i < 32u; ++i) {
    }
}

static void tm1621b_wr_clock(void)
{
    gpio_clear(PIN_TM1621_WR);
    tm1621b_delay();
    gpio_set(PIN_TM1621_WR);
    tm1621b_delay();
}

static void tm1621b_write_bit(bool one)
{
    if (one) {
        gpio_set(PIN_TM1621_DATA);
    } else {
        gpio_clear(PIN_TM1621_DATA);
    }

    tm1621b_delay();
    tm1621b_wr_clock();
}

static void tm1621b_write_bits(uint32_t value, uint8_t count)
{
    uint8_t i = count;

    while (i > 0u) {
        --i;
        tm1621b_write_bit(((value >> i) & 1u) != 0u);
    }
}

static void tm1621b_begin(void)
{
    gpio_set(PIN_TM1621_WR);
    gpio_clear(PIN_TM1621_CS);
    tm1621b_delay();
}

static void tm1621b_end(void)
{
    gpio_set(PIN_TM1621_CS);
    tm1621b_delay();
}

static void tm1621b_send_command(uint8_t command)
{
    tm1621b_begin();
    tm1621b_write_bits(TM1621B_CMD_PREFIX, 4u);
    tm1621b_write_bits(command, 8u);
    tm1621b_end();
}

static void tm1621b_write_address(uint8_t address, uint8_t nibble)
{
    tm1621b_begin();
    tm1621b_write_bits(TM1621B_WR_PREFIX, 3u);
    tm1621b_write_bits(address & 0x3Fu, 6u);
    tm1621b_write_bits(nibble & 0x0Fu, 4u);
    tm1621b_end();
}

void tm1621b_init(void)
{
    uint8_t i;

    gpio_output(PIN_TM1621_CS);
    gpio_output(PIN_TM1621_WR);
    gpio_output(PIN_TM1621_DATA);

    gpio_set(PIN_TM1621_CS);
    gpio_set(PIN_TM1621_WR);
    gpio_set(PIN_TM1621_DATA);

    for (i = 0u; i < TM1621B_SEG_COUNT; ++i) {
        g_tm1621b_ram[i] = 0u;
    }

    tm1621b_send_command(TM1621B_CMD_SYS_DIS);
    tm1621b_send_command(TM1621B_CMD_RC_256K);
    tm1621b_send_command(TM1621B_CMD_BIAS_1_3_4_COM);
    tm1621b_send_command(TM1621B_CMD_SYS_EN);
    tm1621b_send_command(TM1621B_CMD_LCD_ON);
    tm1621b_flush();
}

void tm1621b_clear(void)
{
    uint8_t i;

    for (i = 0u; i < TM1621B_SEG_COUNT; ++i) {
        g_tm1621b_ram[i] = 0u;
    }
}

void tm1621b_set(uint8_t seg, uint8_t com, bool on)
{
    if ((seg >= TM1621B_SEG_COUNT) || (com >= TM1621B_COM_COUNT)) {
        return;
    }

    if (on) {
        g_tm1621b_ram[seg] |= (uint8_t)(1u << com);
    } else {
        g_tm1621b_ram[seg] &= (uint8_t)~(1u << com);
    }
}

void tm1621b_flush(void)
{
    uint8_t addr;

    for (addr = 0u; addr < TM1621B_SEG_COUNT; ++addr) {
        tm1621b_write_address(addr, g_tm1621b_ram[addr]);
    }
}

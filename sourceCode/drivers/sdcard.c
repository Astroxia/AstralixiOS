//
//  PicoCalc SD Card driver - Ported to Luckfox Lyra (RK3506)
//
//  This driver allows file systems to talk to the SD Card at the
//  block-level. Ported from Pico SDK to RK3506 bare-metal.
//
//  TODO: Replace placeholder functions with actual RK3506 SDK calls
//

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: Include your RK3506 SDK headers
// #include "rk3506_spi.h"
// #include "rk3506_gpio.h"

#include "sdcard.h"

// Global state
static bool sd_initialised = false;
static bool is_sdhc = false;
static uint8_t dummy_bytes[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//
// TODO: Implement these based on your RK3506 SDK
//

static inline void gpio_set_value(uint32_t pin, uint8_t value) {
    // TODO: Set GPIO pin value
}

static inline uint8_t gpio_get_value(uint32_t pin) {
    // TODO: Read GPIO pin value
    return 0;
}

static inline void busy_wait_us(uint32_t us) {
    // TODO: Microsecond delay
    volatile uint32_t count = us * 100;
    while(count--);
}

static inline void spi_write_read_blocking(const uint8_t *tx, uint8_t *rx, size_t len) {
    // TODO: SPI transfer function
    // Send tx data and receive into rx buffer
}

static inline void spi_write_blocking(const uint8_t *tx, size_t len) {
    // TODO: SPI write-only function
}

static inline void spi_init_hw(uint32_t baudrate) {
    // TODO: Initialize SPI peripheral with baudrate
}

static inline void spi_set_baudrate_hw(uint32_t baudrate) {
    // TODO: Change SPI baudrate
}

//
// Low-level SD card SPI functions
//

static void sd_spi_write_buf(const uint8_t *src, size_t len)
{
    spi_write_blocking(src, len);
}

static inline void sd_cs_select(void)
{
    gpio_set_value(SD_CS, 0);
    sd_spi_write_buf(dummy_bytes, 8);
}

static inline void sd_cs_deselect(void)
{
    gpio_set_value(SD_CS, 1);
    sd_spi_write_buf(dummy_bytes, 8);
}

static uint8_t sd_spi_write_read(uint8_t data)
{
    uint8_t result;
    spi_write_read_blocking(&data, &result, 1);
    return result;
}

static void sd_spi_read_buf(uint8_t *dst, size_t len)
{
    memset(dst, 0xFF, len);
    spi_write_read_blocking(dst, dst, len);
}

static bool sd_wait_ready(void)
{
    uint8_t response;
    uint32_t timeout = 10000;
    do
    {
        response = sd_spi_write_read(0xFF);
        timeout--;
        if (timeout == 0)
        {
            return false;
        }
    } while (response != 0xFF);
    return true;
}

static uint8_t sd_send_command(uint8_t cmd, uint32_t arg)
{
    uint8_t response;
    uint8_t retry = 0;

    uint8_t packet[6];
    packet[0] = 0x40 | cmd;
    packet[1] = (arg >> 24) & 0xFF;
    packet[2] = (arg >> 16) & 0xFF;
    packet[3] = (arg >> 8) & 0xFF;
    packet[4] = arg & 0xFF;

    uint8_t crc = 0xFF;
    if (cmd == SD_CMD0)
    {
        crc = 0x95;
    }
    if (cmd == SD_CMD8)
    {
        crc = 0x87;
    }
    packet[5] = crc;

    sd_cs_select();
    sd_spi_write_buf(packet, 6);

    response = 0xFF;
    do
    {
        response = sd_spi_write_read(0xFF);
        retry++;
    } while ((response & 0x80) && (retry < 64));

    return response;
}

//
// Card detection and initialisation
//

bool sd_card_present(void)
{
    return !gpio_get_value(SD_DETECT); // Active low
}

bool sd_is_sdhc(void)
{
    return is_sdhc;
}

//
// Block-level read/write operations
//

sd_error_t sd_read_block(uint32_t block, uint8_t *buffer)
{
    int32_t addr = is_sdhc ? block : block * SD_BLOCK_SIZE;
    uint8_t response = sd_send_command(SD_CMD17, addr);
    if (response != 0)
    {
        sd_cs_deselect();
        return SD_ERROR_READ_FAILED;
    }

    uint32_t timeout = 100000;
    do
    {
        response = sd_spi_write_read(0xFF);
        timeout--;
    } while (response != SD_DATA_START_BLOCK && timeout > 0);

    if (timeout == 0)
    {
        sd_cs_deselect();
        return SD_ERROR_READ_FAILED;
    }

    sd_spi_read_buf(buffer, SD_BLOCK_SIZE);

    sd_spi_write_read(0xFF);
    sd_spi_write_read(0xFF);

    sd_cs_deselect();
    return SD_OK;
}

sd_error_t sd_write_block(uint32_t block, const uint8_t *buffer)
{
    uint32_t addr = is_sdhc ? block : block * SD_BLOCK_SIZE;
    uint8_t response = sd_send_command(SD_CMD24, addr);
    if (response != 0)
    {
        sd_cs_deselect();
        return SD_ERROR_WRITE_FAILED;
    }

    sd_spi_write_read(SD_DATA_START_BLOCK);
    sd_spi_write_buf(buffer, SD_BLOCK_SIZE);

    sd_spi_write_read(0xFF);
    sd_spi_write_read(0xFF);

    response = sd_spi_write_read(0xFF) & 0x1F;
    sd_cs_deselect();

    if (response != 0x05)
    {
        return SD_ERROR_WRITE_FAILED;
    }

    sd_cs_select();
    sd_wait_ready();
    sd_cs_deselect();

    return SD_OK;
}

sd_error_t sd_read_blocks(uint32_t start_block, uint32_t num_blocks, uint8_t *buffer)
{
    for (uint32_t i = 0; i < num_blocks; i++)
    {
        sd_error_t result = sd_read_block(start_block + i, buffer + (i * SD_BLOCK_SIZE));
        if (result != SD_OK)
        {
            return result;
        }
    }
    return SD_OK;
}

sd_error_t sd_write_blocks(uint32_t start_block, uint32_t num_blocks, const uint8_t *buffer)
{
    for (uint32_t i = 0; i < num_blocks; i++)
    {
        sd_error_t result = sd_write_block(start_block + i, buffer + (i * SD_BLOCK_SIZE));
        if (result != SD_OK)
        {
            return result;
        }
    }
    return SD_OK;
}

//
// Utility functions
//

const char *sd_error_string(sd_error_t error)
{
    switch (error)
    {
    case SD_OK:
        return "Success";
    case SD_ERROR_NO_CARD:
        return "No SD card present";
    case SD_ERROR_INIT_FAILED:
        return "SD card initialization failed";
    case SD_ERROR_READ_FAILED:
        return "Read operation failed";
    case SD_ERROR_WRITE_FAILED:
        return "Write operation failed";
    default:
        return "Unknown error";
    }
}

//
// Initialisation functions
//

sd_error_t sd_card_init(void)
{
    spi_init_hw(SD_INIT_BAUDRATE);

    sd_cs_deselect();
    busy_wait_us(10000);

    for (int i = 0; i < 80; i++)
    {
        sd_spi_write_read(0xFF);
    }

    busy_wait_us(10000);

    uint8_t response;
    int cmd0_attempts = 0;
    do
    {
        response = sd_send_command(SD_CMD0, 0);
        sd_cs_deselect();
        cmd0_attempts++;
        if (response != SD_R1_IDLE_STATE && cmd0_attempts < 10)
        {
            busy_wait_us(10000);
        }
    } while (response != SD_R1_IDLE_STATE && cmd0_attempts < 10);

    if (response != SD_R1_IDLE_STATE)
    {
        return SD_ERROR_INIT_FAILED;
    }

    response = sd_send_command(SD_CMD8, 0x1AA);
    if (response == SD_R1_IDLE_STATE)
    {
        uint8_t r7[4];
        sd_spi_read_buf(r7, 4);
        sd_cs_deselect();

        if ((r7[2] & 0x0F) != 0x01 || r7[3] != 0xAA)
        {
            return SD_ERROR_INIT_FAILED;
        }
    }
    else
    {
        sd_cs_deselect();
    }

    uint32_t timeout = 1000;
    do
    {
        response = sd_send_command(SD_CMD55, 0);
        sd_cs_deselect();

        if (response > 1)
        {
            return SD_ERROR_INIT_FAILED;
        }

        response = sd_send_command(SD_ACMD41, 0x40000000);
        sd_cs_deselect();

        if (response == 0)
        {
            break;
        }

        busy_wait_us(1000);
        timeout--;
    } while (timeout > 0);

    if (timeout == 0)
    {
        return SD_ERROR_INIT_FAILED;
    }

    response = sd_send_command(SD_CMD58, 0);
    if (response != 0)
    {
        sd_cs_deselect();
        return SD_ERROR_INIT_FAILED;
    }
    uint8_t ocr[4] = {0};
    sd_spi_read_buf(ocr, 4);
    sd_cs_deselect();

    is_sdhc = (ocr[0] & 0x40) != 0;

    if (!is_sdhc)
    {
        response = sd_send_command(SD_CMD16, SD_BLOCK_SIZE);
        sd_cs_deselect();

        if (response != 0)
        {
            return SD_ERROR_INIT_FAILED;
        }
    }

    spi_set_baudrate_hw(SD_BAUDRATE);

    return SD_OK;
}

void sd_init(void)
{
    if (sd_initialised)
    {
        return;
    }

    // TODO: Initialize GPIO pins for SD card
    // gpio_init_output(SD_CS);
    // gpio_init_input(SD_DETECT);
    // gpio_set_pull(SD_DETECT, GPIO_PULL_UP);
    
    // TODO: Configure SPI pins
    // set_pin_function(SD_MISO, PIN_FUNC_SPI);
    // set_pin_function(SD_SCK, PIN_FUNC_SPI);
    // set_pin_function(SD_MOSI, PIN_FUNC_SPI);

    sd_initialised = true;
}
#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "nrf_drv_spi.h"
#include "bsp.h"
#include "app_timer.h"
#include "nordic_common.h"

#include "spi_flash.h"

#define WAIT_FOR_TRANSFER_COMPLETED                         \
	do                                                      \
    {                                                       \
        while(!m_transfer_completed);                       \
        m_transfer_completed = false;                       \
    } while (0)    


#define min(a,b)  (a<b)?a:b


#define FLASH_SPI_A0_PIN        7     
#define FLASH_SPI_SCK_PIN       9    
#define FLASH_SPI_MOSI_PIN      10   
#define FLASH_SPI_MISO_PIN      NRF_DRV_SPI_PIN_NOT_USED
#define FLASH_SPI_SS_PIN        8    


static volatile bool m_transfer_completed = true;
static struct spi_flash g_spi_flash;

static void spi_flash_addr(u32 addr, u8 *cmd)
{
	/* cmd[0] is actual command */
	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr >> 0;
}

static int spi_flash_read_write(struct spi_slave *spi,
				const u8 *cmd, size_t cmd_len,
				const u8 *data_out, u8 *data_in,
				size_t data_len)
{
	int ret = -1;

    WAIT_FOR_TRANSFER_COMPLETED;
	if (nrf_drv_spi_transfer(spi->drv_spi,
        cmd, cmd_len, NULL, 0) != NRF_SUCCESS) {
        goto exit;
	}

    if (data_len > 0) {
        WAIT_FOR_TRANSFER_COMPLETED;
    	if (nrf_drv_spi_transfer(spi->drv_spi,
            data_out, data_len, data_in, data_len) != NRF_SUCCESS) {
            goto exit;
    	}
    }
	ret = 0;
exit:
	return ret;
}

static int spi_flash_cmd_read(struct spi_slave *spi, const u8 *cmd,
		size_t cmd_len, void *data, size_t data_len)
{
	return spi_flash_read_write(spi, cmd, cmd_len, NULL, data, data_len);
}

static int spi_flash_cmd(struct spi_slave *spi, u8 cmd, void *response, size_t len)
{
	return spi_flash_cmd_read(spi, &cmd, 1, response, len);
}

static int spi_flash_cmd_write(struct spi_slave *spi, const u8 *cmd, size_t cmd_len,
		const void *data, size_t data_len)
{
	return spi_flash_read_write(spi, cmd, cmd_len, data, NULL, data_len);
}

static int spi_flash_cmd_wait_ready(struct spi_flash *flash, unsigned long timeout)
{
	struct spi_slave *spi = flash->spi;
	int ret = -1;
	u8 status;
	u8 check_status = 0x0;
	u8 poll_bit = STATUS_WIP;
	u8 cmd = flash->poll_cmd;

	if (cmd == CMD_FLAG_STATUS) {
		poll_bit = STATUS_PEC;
		check_status = poll_bit;
	}

    WAIT_FOR_TRANSFER_COMPLETED;
	if (nrf_drv_spi_transfer(spi->drv_spi,
        &cmd, 1, NULL, 0) != NRF_SUCCESS) {
        goto exit;
	}

	do {
        WAIT_FOR_TRANSFER_COMPLETED;
    	if (nrf_drv_spi_transfer(spi->drv_spi,
            NULL, 0, &status, 1) != NRF_SUCCESS) {
            goto exit;
    	}
		if ((status & poll_bit) == check_status)
			break;

	} while (timeout--);

	if ((status & poll_bit) == check_status) {
		ret = 0;
	}
exit:
	return ret;
}

static int spi_flash_write_common(struct spi_flash *flash, const u8 *cmd,
		size_t cmd_len, const void *buf, size_t buf_len)
{
	struct spi_slave *spi = flash->spi;
	unsigned long timeout = SPI_FLASH_PROG_TIMEOUT;
	int ret;

	if (buf == NULL)
		timeout = SPI_FLASH_PAGE_ERASE_TIMEOUT;

	ret = spi_flash_cmd_write(spi, cmd, cmd_len, buf, buf_len);
	if (ret < 0) {
		return ret;
	}

	ret = spi_flash_cmd_wait_ready(flash, timeout);
	if (ret < 0) {
		return ret;
	}
	return ret;
}

static int spi_flash_cmd_erase(struct spi_flash *flash, u32 offset, size_t len)
{
	u32 erase_size;
	u8 cmd[4];
	int ret = -1;

	erase_size = flash->sector_size;
	if (offset % erase_size || len % erase_size) {
		return -1;
	}

	if (erase_size == 4096)
		cmd[0] = CMD_ERASE_4K;
	else
		cmd[0] = CMD_ERASE_64K;

	while (len) {
		spi_flash_addr(offset, cmd);

		ret = spi_flash_write_common(flash, cmd, sizeof(cmd), NULL, 0);
		if (ret < 0) {
			break;
		}

		offset += erase_size;
		len -= erase_size;
	}

	return ret;
}

static int spi_flash_cmd_write_multi(struct spi_flash *flash, u32 offset,
		size_t len, const u8 *buf)
{
	unsigned long byte_addr, page_size;
	size_t chunk_len, actual;
	u8 cmd[4];
	int ret = -1;

	page_size = flash->page_size;

	cmd[0] = CMD_PAGE_PROGRAM;
	for (actual = 0; actual < len; actual += chunk_len) {
		byte_addr = offset % page_size;
		chunk_len = min(len - actual, page_size - byte_addr);

		if (flash->spi->max_write_size)
			chunk_len = min(chunk_len, flash->spi->max_write_size);

		spi_flash_addr(offset, cmd);

		ret = spi_flash_write_common(flash, cmd, sizeof(cmd),
					buf + actual, chunk_len);
		if (ret < 0) {
			break;
		}

		offset += chunk_len;
	}

	return ret;
}

static int spi_flash_read_common(struct spi_flash *flash, const u8 *cmd,
		size_t cmd_len, void *data, size_t data_len)
{
	struct spi_slave *spi = flash->spi;
	int ret;

	ret = spi_flash_cmd_read(spi, cmd, cmd_len, data, data_len);
	if (ret < 0) {
		return ret;
	}
	return ret;
}

static int spi_flash_cmd_read_fast(struct spi_flash *flash, u32 offset,
		size_t len, u8 *data)
{
	u8 cmd[5], bank_sel = 0;
	u32 remain_len, read_len;
	int ret = -1;

	cmd[0] = CMD_READ_ARRAY_FAST;
	cmd[4] = 0x00;

	while (len) {
		remain_len = (SPI_FLASH_16MB_BOUN * (bank_sel + 1) - offset);
		if (len < remain_len)
			read_len = len;
		else
			read_len = remain_len;

		spi_flash_addr(offset, cmd);

		ret = spi_flash_read_common(flash, cmd, sizeof(cmd),
							data, read_len);
		if (ret < 0) {
			break;
		}

		offset += read_len;
		len -= read_len;
		data += read_len;
	}

	return ret;
}


static int spi_flash_probe(struct spi_flash *flash, const char *name)
{
	int ret;
	u8 idcode[3];

	/* Read the ID codes */
	ret = spi_flash_cmd(flash->spi, CMD_READ_ID, idcode, sizeof(idcode));
	if (ret) {
		return -1;
	}
	if (idcode[0] != SPI_FLASH_WINBOND_IDCODE0) {
		return -1;
	}
    flash->name = name;
	flash->page_size = 256;
	flash->sector_size = (idcode[1] == 0x20) ? 65536 : 4096;
	flash->size = 4096 * 16 * 128; /* sector size * 16(sectors per block) * blocks */
	flash->name = name;
	flash->poll_cmd = CMD_READ_STATUS;

	flash->read = spi_flash_cmd_read_fast;
	flash->write = spi_flash_cmd_write_multi;
	flash->erase = spi_flash_cmd_erase;

	return 0;
}


static void spi_flash_event_handler(nrf_drv_spi_event_t event)
{
    switch (event)
    {
        case NRF_DRV_SPI_EVENT_DONE:
            m_transfer_completed = true;
            break;
        default:
            // No implementation needed.
            break;
    }
}

int spi_flash_init(void)
{

    static nrf_drv_spi_t drv_spi = NRF_DRV_SPI_INSTANCE(1);
	  static struct spi_slave spi;
    static const char name[] = "W25Q64CV/W25Q64FV_SPI";
    spi.drv_spi = &drv_spi;
	  spi.max_write_size = 256;

    nrf_drv_spi_config_t config =
    {
        .ss_pin       = FLASH_SPI_SS_PIN,
        .irq_priority = APP_IRQ_PRIORITY_LOW,
        .orc          = 0xCC,
        .frequency    = NRF_DRV_SPI_FREQ_8M,
        .mode         = NRF_DRV_SPI_MODE_0,
        .bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,//NRF_DRV_SPI_BIT_ORDER_LSB_FIRST,
    };
    config.sck_pin  = FLASH_SPI_SCK_PIN;
    config.mosi_pin = FLASH_SPI_MOSI_PIN;
    config.miso_pin = FLASH_SPI_MISO_PIN;
    if (nrf_drv_spi_init(&drv_spi, &config,
        spi_flash_event_handler) != NRF_SUCCESS) {
        return -1;
    }
    g_spi_flash.spi = &spi;
    return spi_flash_probe(&g_spi_flash, name);
}

int spi_flash_read(unsigned int offset, size_t len, unsigned char *data)
{
    return g_spi_flash.read(&g_spi_flash, offset, len, data);
}

int spi_flash_write(unsigned int offset, size_t len, const unsigned char *buf)
{
    return g_spi_flash.write(&g_spi_flash, offset, len, buf);
}

int spi_flash_erase(unsigned int offset, size_t len)
{
    return g_spi_flash.erase(&g_spi_flash, offset, len);
}


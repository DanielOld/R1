#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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


#define FLASH_SPI_SCK_PIN       2
#define FLASH_SPI_MOSI_PIN      5
#define FLASH_SPI_MISO_PIN      4
#define FLASH_SPI_SS_PIN        3
#define SPI_FLASH_MAX_READ_WRITE_SIZE    251


static volatile bool m_transfer_completed = true;
static struct spi_flash g_spi_flash;

static void spi_flash_addr(u32 addr, u8 *cmd)
{
	/* cmd[0] is actual command */
	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr >> 0;
}

static int spi_flash_cmd_wait_ready(struct spi_flash *flash, unsigned long timeout)
{
	u8 cmd;
	u8 status[3];

	do {
		WAIT_FOR_TRANSFER_COMPLETED;
		cmd = CMD_READ_STATUS;
		if(nrf_drv_spi_transfer(flash->spi->drv_spi,
		                        &cmd, 1, status, 3) != NRF_SUCCESS) {
			return -1;
		}
		if((status[1]&0x01) == 0) {
			return 0;
		}
	} while(timeout--);

	return -1;
}


static int spi_flash_cmd_erase(struct spi_flash *flash, u32 offset, size_t len)
{
	u32 erase_size;
	u8 cmd[4];

	erase_size = flash->sector_size;
	if(offset % erase_size || len % erase_size) {
		return -1;
	}

	if(erase_size == 4096) {
		cmd[0] = CMD_ERASE_4K;
	} else {
		cmd[0] = CMD_ERASE_64K;
	}

	while(len) {
		spi_flash_addr(offset, cmd);
		if(nrf_drv_spi_transfer(flash->spi->drv_spi,
		                        cmd, 4, NULL, 0) != NRF_SUCCESS) {
			return -1;
		}
		if(spi_flash_cmd_wait_ready(flash, SPI_FLASH_SECTOR_ERASE_TIMEOUT) != 0) {
			return -1;
		}
		offset += erase_size;
		len -= erase_size;
	}

	return 0;
}

static int spi_flash_cmd_write_multi(struct spi_flash *flash, u32 offset,
                                     size_t len, const u8 *data)
{
	unsigned long byte_addr;
	unsigned long page_size;
	size_t chunk_len;
	size_t actual;
	u8* cmd;
	u8 buf[256];
	int ret = -1;

	page_size = flash->page_size;

	cmd = buf;
	cmd[0] = CMD_PAGE_PROGRAM;
	for(actual = 0; actual < len; actual += chunk_len) {
		byte_addr = offset % page_size;
		chunk_len = min(len - actual, page_size - byte_addr);

		if(flash->spi->max_write_size) {
			chunk_len = min(chunk_len, flash->spi->max_write_size);
		}
		spi_flash_addr(offset, cmd);

		memcpy(&buf[4], data+actual, chunk_len);
		WAIT_FOR_TRANSFER_COMPLETED;
		if(nrf_drv_spi_transfer(flash->spi->drv_spi,
		                        buf, chunk_len+4, NULL, 0) != NRF_SUCCESS) {
			return -1;
		}
		offset += chunk_len;
		if(spi_flash_cmd_wait_ready(flash, SPI_FLASH_PROG_TIMEOUT) != 0) {
			return -1;
		}
	}

	return ret;
}

static int spi_flash_cmd_read_fast(struct spi_flash *flash, u32 offset,
                                   size_t len, u8 *data)
{
	u8 cmd[5];
	u8 buf[256];
	u32 read_len;

	cmd[0] = CMD_READ_ARRAY_FAST;
	cmd[4] = 0x00;

	while(len) {
		read_len = min(len, flash->spi->max_write_size);
		spi_flash_addr(offset, cmd);
		WAIT_FOR_TRANSFER_COMPLETED;
		if(nrf_drv_spi_transfer(flash->spi->drv_spi,
		                        cmd, 5, buf, read_len+5) != NRF_SUCCESS) {
			return -1;
		}
		offset += read_len;
		len -= read_len;
		memcpy(data, &buf[5], read_len);
		data += read_len;
	}

	return 0;
}

static int spi_flash_probe(struct spi_flash *flash, const char *name)
{
	u8 cmd;
	u8 idcode[4];


	WAIT_FOR_TRANSFER_COMPLETED;
	cmd = CMD_READ_ID;
	if(nrf_drv_spi_transfer(flash->spi->drv_spi,
	                        &cmd, 1, idcode, 4) != NRF_SUCCESS) {
		return -1;
	}
	if(idcode[1] != SPI_FLASH_WINBOND_IDCODE0) {
		return -1;
	}
	flash->name = name;
	flash->page_size = 256;
	flash->sector_size = (idcode[2] == 0x20) ? 65536 : 4096;
	flash->size = 4096 * 16 * 128; /* sector size * 16(sectors per block) * blocks */
	flash->name = name;

	return 0;
}


static void spi_flash_event_handler(nrf_drv_spi_event_t event)
{
	switch(event) {
		case NRF_DRV_SPI_EVENT_DONE:
			m_transfer_completed = true;
			break;
		default:
			// No implementation needed.
			break;
	}
}
int spi_flash_test(void);
u8 m_data[256];

int spi_flash_init(void)
{
	int ret;
	static nrf_drv_spi_t drv_spi = NRF_DRV_SPI_INSTANCE(1);
	static struct spi_slave spi;
	static const char name[] = "W25Q64CV/W25Q64FV_SPI";
	spi.drv_spi = &drv_spi;
	spi.max_write_size = SPI_FLASH_MAX_READ_WRITE_SIZE;

	nrf_drv_spi_config_t config = {
		.ss_pin       = FLASH_SPI_SS_PIN,
		.irq_priority = APP_IRQ_PRIORITY_LOW,
		.orc          = 0xCC,
		.frequency    = NRF_DRV_SPI_FREQ_1M,
		.mode         = NRF_DRV_SPI_MODE_0,
		.bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,//NRF_DRV_SPI_BIT_ORDER_LSB_FIRST,
	};
	config.sck_pin  = FLASH_SPI_SCK_PIN;
	config.mosi_pin = FLASH_SPI_MOSI_PIN;
	config.miso_pin = FLASH_SPI_MISO_PIN;
	if(nrf_drv_spi_init(&drv_spi, &config,
	                    spi_flash_event_handler) != NRF_SUCCESS) {
		return -1;
	}
	g_spi_flash.spi = &spi;

	ret = spi_flash_probe(&g_spi_flash, name);
	spi_flash_test();

	return ret;
}

int spi_flash_read(unsigned int offset, size_t len, unsigned char *data)
{
	return spi_flash_cmd_read_fast(&g_spi_flash, offset, len, data);
}

int spi_flash_write(unsigned int offset, size_t len, const unsigned char *data)
{
	return spi_flash_cmd_write_multi(&g_spi_flash, offset, len, data);
}

int spi_flash_erase(unsigned int offset, size_t len)
{
	return spi_flash_cmd_erase(&g_spi_flash, offset, len);
}


int spi_flash_test(void)
{
	int i;

	if(spi_flash_erase(0, 4096*2) != 0) {
		return -1;
	}
	for(i = 0; i<sizeof(m_data); i++) {
		m_data[i] = i;
	}
	if(spi_flash_write(0, 256, m_data) != 0) {
		return -1;
	}
	memset(m_data, 0, sizeof(m_data));
	if(spi_flash_read(0, 256, m_data) != 0) {
		return -1;
	}

	return 0;
}

/*
 * Interface to SPI flash
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

#include "nrf_drv_spi.h"
#include "ring_types.h"


#define SPI_FLASH_PROG_TIMEOUT		(2000)
#define SPI_FLASH_PAGE_ERASE_TIMEOUT	(5000)
#define SPI_FLASH_SECTOR_ERASE_TIMEOUT	(10000)

/* Common commands */
#define CMD_READ_ARRAY_FAST		0x0b

#define CMD_READ_ID			    0x9f
#define CMD_PAGE_PROGRAM		0x02
#define CMD_READ_STATUS			0x05
#define CMD_ERASE_4K			0x20
#define CMD_ERASE_64K			0xd8
#define CMD_ERASE_CHIP			0xc7

#define SPI_FLASH_16MB_BOUN		0x1000000

/* Manufacture ID's */
#define SPI_FLASH_WINBOND_IDCODE0	0xef

struct spi_slave {
	nrf_drv_spi_t* drv_spi;
	unsigned int max_write_size;
};

struct spi_flash {
	struct spi_slave *spi;

	const char	*name;

	/* Total flash size */
	u32		size;
	/* Write (page) size */
	u32		page_size;
	/* Erase (sector) size */
	u32		sector_size;
};

int spi_flash_init(void);
int spi_flash_read(unsigned int offset, size_t len, unsigned char *data);
int spi_flash_write(unsigned int offset, size_t len, const unsigned char *data);
int spi_flash_erase(unsigned int offset, size_t len);

#endif /* _SPI_FLASH_H_ */

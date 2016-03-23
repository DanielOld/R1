#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "nrf_drv_spi.h"
#include "bsp.h"
#include "app_timer.h"
#include "nordic_common.h"

#include "lcd.h"

#define LCD_ENABLE_PIN        22
#define LCD_RESET_PIN_NUMBER  6
#define LCD_SPI_A0_PIN        7
#define LCD_SPI_SCK_PIN       9
#define LCD_SPI_MOSI_PIN      10
#define LCD_SPI_MISO_PIN      NRF_DRV_SPI_PIN_NOT_USED
#define LCD_SPI_SS_PIN        8

#define LCD_ENABLE_PIN_CONFIG() do { \
        NRF_GPIO->PIN_CNF[LCD_ENABLE_PIN] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) \
        |(GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)    \
        |(GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos)  \
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) \
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  \
} while (0)

#define LCD_ENABLE_HIGH()   do { NRF_GPIO->OUTSET = (1UL << LCD_ENABLE_PIN); } while(0)

#define LCD_RESET_PIN_CONFIG() do { \
        NRF_GPIO->PIN_CNF[LCD_RESET_PIN_NUMBER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) \
        |(GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)    \
        |(GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos)  \
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) \
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  \
} while (0)

#define LCD_A0_PIN_CONFIG() do { \
        NRF_GPIO->PIN_CNF[LCD_SPI_A0_PIN] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) \
        |(GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)    \
        |(GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos)  \
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) \
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  \
} while (0)

#define LCD_RESET_HIGH()   do { NRF_GPIO->OUTSET = (1UL << LCD_RESET_PIN_NUMBER); } while(0)
#define LCD_RESET_LOW()    do { NRF_GPIO->OUTCLR = (1UL << LCD_RESET_PIN_NUMBER); } while(0)
#define LCD_SPI_A0_HIGH()   do { NRF_GPIO->OUTSET = (1UL << LCD_SPI_A0_PIN); } while(0)
#define LCD_SPI_A0_LOW()    do { NRF_GPIO->OUTCLR = (1UL << LCD_SPI_A0_PIN); } while(0)


static volatile bool m_lcd_transfer_completed = true;
static const nrf_drv_spi_t m_lcd_spi_master = NRF_DRV_SPI_INSTANCE(0);
static uint8_t m_lcd_buffer[LCD_BUFFER_SIZE];
static void lcd_clear(uint16_t color);

static void lcd_spi_master_event_handler(nrf_drv_spi_event_t event)
{
	switch(event) {
		case NRF_DRV_SPI_EVENT_DONE:
			m_lcd_transfer_completed = true;
			break;
		default:
			// No implementation needed.
			break;
	}
}

static void lcd_spi_master_init(void)
{
	uint32_t err_code = NRF_SUCCESS;

	nrf_drv_spi_config_t config = {
		.ss_pin       = LCD_SPI_SS_PIN,
		.irq_priority = APP_IRQ_PRIORITY_LOW,
		.orc          = 0xCC,
		.frequency    = NRF_DRV_SPI_FREQ_8M,
		.mode         = NRF_DRV_SPI_MODE_0,
		.bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,//NRF_DRV_SPI_BIT_ORDER_LSB_FIRST,
	};
	config.sck_pin  = LCD_SPI_SCK_PIN;
	config.mosi_pin = LCD_SPI_MOSI_PIN;
	config.miso_pin = LCD_SPI_MISO_PIN;
	err_code = nrf_drv_spi_init(&m_lcd_spi_master, &config,
	                            lcd_spi_master_event_handler);
	APP_ERROR_CHECK(err_code);
}

static void write_c(uint8_t cmd)
{
	uint32_t err_code;

	while(!m_lcd_transfer_completed);
	LCD_SPI_A0_LOW();
	m_lcd_transfer_completed = false;
	err_code = nrf_drv_spi_transfer(&m_lcd_spi_master,
	                                &cmd, 1, NULL, 0);
	APP_ERROR_CHECK(err_code);
}
static void write_d(uint8_t data)
{
	uint32_t err_code;

	while(!m_lcd_transfer_completed);
	LCD_SPI_A0_HIGH();
	m_lcd_transfer_completed = false;
	err_code = nrf_drv_spi_transfer(&m_lcd_spi_master,
	                                &data, 1, NULL, 0);
	APP_ERROR_CHECK(err_code);
}

void lcd_init(void)
{
	lcd_spi_master_init();
    LCD_ENABLE_PIN_CONFIG();
	LCD_ENABLE_HIGH();
	LCD_A0_PIN_CONFIG();
	LCD_RESET_PIN_CONFIG();
	LCD_RESET_LOW();
	nrf_delay_ms(100);
	LCD_RESET_HIGH();
	nrf_delay_ms(100);
	write_c(0x01); /* SET SOFT RESET */
	write_d(0x00);
	write_c(0x14); /* STANDBY ON/OFF */
	write_d(0x00);
	write_c(0x02); /* DISPLAY ON/OFF */
	write_d(0x00);
	write_c(0x0F); /* Set ANALOG_CONTROL */
	write_d(0x40);
	write_c(0x1A); /* OSC ADJUST */
	write_d(0x03);
	write_c(0x38); /*DISPLAY START */
	write_d(0x00);
	write_c(0x39);
	write_d(0x00);
	write_c(0xE0); /*RGB IF*/
	write_d(0x00);
	write_c(0xE1); /*Set RGB_POL*/
	write_d(0x00);
	write_c(0xE5); /*DISPLAY MODE CONTROL*/
	//write_d(0x00);
	write_d(0x00); /*Do not swap RGB*/
	write_c(0x0D); /* CPU IF */
	write_d(0x00);
	write_c(0x1D); /* Set MEMORY_WRITE/READ */
	write_d(0x00); /* 0x03 */
	write_c(0x09); /* Set ROW_SCAN_DIRECTION */
	write_d(0x00); /* 0x02 */
	write_c(0x13); /* Set ROW_SCAN_MODE */
	write_d(0x00);
	write_c(0x40); /*COLUMN CURRENT R */
	write_d(0x7F);
	write_c(0x41); /*COLUMN CURRENT G */
	write_d(0x65);
	write_c(0x42); /*COLUMN CURRENT B*/
	write_d(0x7B);
	write_c(0x48); /*ROW OVERLAP */
	write_d(0x03);
	write_c(0x18); /* DISCHARGE TIME */
	write_d(0x03);
	write_c(0x16); /*PEAK PULSE DELAY */
	write_d(0x00);
	write_c(0x3A); /*PEAK PULSE WIDTH R */
	write_d(0x03);
	write_c(0x3B); /*PEAK PULSE WIDTH G */
	write_d(0x03);
	write_c(0x3C); /*PEAK PULSE WIDTH B */
	write_d(0x03);
	write_c(0x3D); /*PRECHARGE CURRENT R*/
	write_d(0x09);
	write_c(0x3E); /*PRECHARGE CURRENT G */
	write_d(0x09);
	write_c(0x3F); /*PRECHARGE CURRENT B */
	write_d(0x08);
	write_c(0x17); /*Set ROW_SCAN_ON/OFF */
	write_d(0x00);
	write_c(0x49); /*SCAN OFF LEVEL */
	write_d(0x0F);
	write_c(0x30); /* Set DISPLAY_X1 */
	write_d(0x00);
	write_c(0x31); /* Set DISPLAY_X2 */
	write_d(0x5F);
	write_c(0x32); /* Set DISPLAY_Y1 */
	write_d(0x00);
	write_c(0x33); /* Set DISPLAY_Y2 */
	write_d(0x5F);
	write_c(0x02); /*DISPLAY ON*/
	write_d(0x01);
	lcd_clear(0xf800);
}

void lcd_sleep(void)
{
	write_c(0x02); /* DISPLAY ON/OFF */
	write_d(0x00);
	write_c(0x14); /* STANDBY ON/ON */
	write_d(0x01);
}

void lcd_wakeup(void)
{
	write_c(0x14); /* STANDBY ON/OFF */
	write_d(0x00);
	write_c(0x02); /*DISPLAY ON/ON*/
	write_d(0x01);
}

uint8_t* lcd_get_buffer_address(void)
{
	return m_lcd_buffer;
}

static void lcd_memory_access(uint8_t const * p_tx_buffer,
                              uint8_t tx_buffer_length)
{
	uint32_t err_code;

	while(!m_lcd_transfer_completed);
	LCD_SPI_A0_HIGH();
	m_lcd_transfer_completed = false;
	err_code = nrf_drv_spi_transfer(&m_lcd_spi_master,
	                                p_tx_buffer, tx_buffer_length, NULL, 0);
	APP_ERROR_CHECK(err_code);
}

void lcd_display(uint8_t startx,
                 uint8_t starty,
                 uint8_t endx,
                 uint8_t endy)
{
#if 0
	uint32_t i;
	uint32_t j;

	if(startx < LCD_WITH &&
	   endx < LCD_WITH &&
	   starty < LCD_HIGHT &&
	   endy < LCD_HIGHT) {
#if 0
		write_c(0x34); /* MEM_X1 */
		write_d(startx);
		write_c(0x35); /* MEM_X2 */
		write_d(endx);
		write_c(0x36); /* MEM_Y1 */
		write_d(starty);
		write_c(0x37); /* MEM_Y2 */
		write_d(endy);
		write_c(0x08); /*memory access*/
#endif
		write_c(0x08); /*memory access*/
		for(i=0; i<(sizeof(m_lcd_buffer)/2); i++) {
			write_d(0xf8);
			write_d(0x00);
		}
		for(i=0; i<(sizeof(m_lcd_buffer)/2); i++) {
			write_d(0x07);
			write_d(0xe0);
		}
		for(i=0; i<(sizeof(m_lcd_buffer)/2); i++) {
			write_d(0x00);
			write_d(0x1f);
		}
	}
#else
	uint32_t i;

	write_c(0x08); /*memory access*/
	for(i=0; i<LCD_HIGHT; i++) {
		lcd_memory_access(m_lcd_buffer, LCD_WITH*2);
	}
#endif
}

static void lcd_clear(uint16_t color)
{
	uint32_t i;

	for(i=0; i<sizeof(m_lcd_buffer); i=i+2) {
		m_lcd_buffer[i] = color & 0xff;
		m_lcd_buffer[i+1] = (color>>8) & 0xff;
	}
	lcd_display(0, LCD_WITH-1, 0, LCD_HIGHT-1);
}

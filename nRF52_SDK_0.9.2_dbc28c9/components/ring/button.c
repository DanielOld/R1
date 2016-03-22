#include "bsp.h"
#include <stddef.h>
#include <stdio.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_error.h"
#include "nrf_drv_gpiote.h"
#include "app_timer.h"

#include "button.h"

#define HALL_INTERVAL_VALUE  APP_TIMER_TICKS(100, 0) 

#define GPIO_BUTTON_OK      30
#define GPIO_BUTTON_CANCEL  27
#define GPIO_HALL_1			18
#define GPIO_HALL_2			17
//#define GPIO_BATTERY_LOW_DETECT      29
//#define GPIO_CHARGE_DONE	28
//#define GPIO_VIBERATE_ENABLE 26


static volatile bool clockwise_event = false;
static volatile bool anticlockwise = false;
static uint32_t m_hall_interval_value = 0;
static uint32_t m_button_value = 0;

static void btn_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
			static uint32_t hall1_tick = 0;
			static uint32_t hall2_tick = 0;

    switch (pin) {
		case GPIO_BUTTON_OK:
			m_button_value = GPIO_BUTTON_OK;
			break;
		case GPIO_BUTTON_CANCEL:
			m_button_value = GPIO_BUTTON_CANCEL;
			break;
		case GPIO_HALL_1:
		        app_timer_cnt_get(&hall1_tick);
					m_hall_interval_value = hall1_tick - hall2_tick;
					if (m_hall_interval_value < HALL_INTERVAL_VALUE) {
						clockwise_event = true;
						anticlockwise = false;
				} 
			break;
		case GPIO_HALL_2:
		        app_timer_cnt_get(&hall2_tick);
					m_hall_interval_value = hall2_tick - hall1_tick;
					if (m_hall_interval_value < HALL_INTERVAL_VALUE) {
						clockwise_event = false;
						anticlockwise = true;
				} 
			break;
		default:
			break;
    }
}

uint32_t btn_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
	
    nrf_drv_gpiote_in_config_t config;

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }


    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_BUTTON_OK, &config, btn_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_BUTTON_OK, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_BUTTON_OK);


    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_BUTTON_CANCEL, &config, btn_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_BUTTON_CANCEL, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_BUTTON_CANCEL);
	
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_HALL_1, &config, btn_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_HALL_1, true);
	
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_HALL_2, &config, btn_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_HALL_2, true);

    return NRF_SUCCESS;
}

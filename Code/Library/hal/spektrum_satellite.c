/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */


/**
* \file remote_dsm2.c
*
* This file is the driver for the remote control
*/


#include "spektrum_satellite.h"
#include "remote_controller.h"

#include "usart.h"

#include "time_keeper.h"
#include "gpio.h"
#include "sysclk.h"

#include "print_util.h"

#include "delay.h"
#include "mavlink_communication.h"


#define REMOTE_UART AVR32_USART1						///< Define the microcontroller pin map with the remote UART
#define DSM_RECEIVER_PIN AVR32_PIN_PD12					///< Define the microcontroller pin map with the receiver pin
#define RECEIVER_POWER_ENABLE_PIN AVR32_PIN_PC01		///< Define the microcontroller pin map with the receiver power enable pin

spektrum_satellite_t sat;								///< Declare an object containing the receiver structure for receiver 1

int16_t channel_center[16];								///< Declare an array to store the central position of each channel

///< Function prototype definitions
int8_t check_receiver1(void);
int8_t check_receiver2(void);


/**
 * \brief Define the service routine for the spektrum handler interruption
 */
ISR(spectrum_handler, AVR32_USART1_IRQ, AVR32_INTC_INTLEV_INT1) 
{
	uint8_t c1, c2, i;
	uint8_t channel_encoding, frame_number;
	uint16_t sw;
	uint8_t channel;
	uint32_t now =time_keeper_get_time_ticks() ;

	if (REMOTE_UART.csr & AVR32_USART_CSR_RXRDY_MASK) 
	{
		sat.duration  = now - sat.last_time;
		sat.last_time = now;

		if ((sat.duration > 2500)) 
		{
			buffer_clear(&sat.receiver);
		}
		c1 = (uint8_t)REMOTE_UART.rhr;
		buffer_put(&sat.receiver, c1);
		
		if ( buffer_bytes_available(&sat.receiver) == 16 ) 
		{
			// first two bytes are status info
			c1 = buffer_get(&sat.receiver);
			c2 = buffer_get(&sat.receiver);
			
			channel_encoding = (c2 & 0x10) >> 4; 	/* 0 = 11bit, 1 = 10 bit */
			frame_number     = c2 & 0x03; 			/* 1 = 1 frame contains all channels */
			
			for (i = 1; i < 8; i++) 
			{
				c1 = buffer_get(&sat.receiver);
				c2 = buffer_get(&sat.receiver);
				sw = (uint16_t)c1 << 8 | ((uint16_t)c2);
				
				//if (c1 & 0x80 == 0)
				
				if ( channel_encoding == 1 ) 
				{
					// highest bit is frame 0/1, bits 2-6 are channel number
					channel = ((c1&0x80) * 8 + (c1 >> 2))&0x0f;
					// 10 bits per channel
					sat.channels[channel] = ((int16_t)(sw&0x3ff) - 512) * 2;
				} 
				else if ( channel_encoding == 0 ) 
				{
					// highest bit is frame 0/1, bits 3-7 are channel number
					channel = ((c1&0x80) * 8 + (c1 >> 3))&0x0f;
					// 11 bits per channel
					sat.channels[channel] = ((int16_t)(sw&0x7ff) -1024);
				} 
				else 
				{
					// shouldn't happen!
				}
				
				//sat.channels[i] = sw&0x3ff;
				sat.valid 		= 1;
				sat.last_update 	= now;
			}
		}
	}		
}


void spektrum_satellite_switch_on(void) 
{
	gpio_configure_pin(RECEIVER_POWER_ENABLE_PIN, GPIO_DIR_OUTPUT);
	gpio_set_pin_low(RECEIVER_POWER_ENABLE_PIN);
}


void spektrum_satellite_switch_off(void) 
{
	gpio_configure_pin(RECEIVER_POWER_ENABLE_PIN, GPIO_DIR_OUTPUT);
	gpio_set_pin_high(RECEIVER_POWER_ENABLE_PIN);
}


void spektrum_satellite_bind() 
{
	int32_t i = 0;
	uint32_t cpu_freq = sysclk_get_cpu_hz();
	
	// Switch off satellite
	spektrum_satellite_switch_off();
	delay_ms(100);
	spektrum_satellite_switch_on();
	
	// Send one first pulse
	gpio_configure_pin(DSM_RECEIVER_PIN, GPIO_DIR_INPUT | GPIO_PULL_DOWN);	
	delay_ms(1);
	gpio_configure_pin(DSM_RECEIVER_PIN, GPIO_DIR_INPUT| GPIO_INIT_LOW);
	delay_ms(10);

	// Wait for startup signal
	while ((gpio_get_pin_value(DSM_RECEIVER_PIN) == 0) && (i < 10000)) 
	{
		i++;
		delay_ms(1);
	}

	// Wait 100ms after receiver startup
	delay_ms(100);

	// create 4 pulses with 126us to set receiver to bind mode
	for (i = 0; i < 3; i++) 
	{
		gpio_configure_pin(DSM_RECEIVER_PIN, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
		cpu_delay_us(113, cpu_freq); 
		gpio_configure_pin(DSM_RECEIVER_PIN, GPIO_DIR_INPUT | GPIO_PULL_UP);	
		cpu_delay_us(118, cpu_freq);
	}
}


void spektrum_satellite_init (void) 
{
	static const usart_options_t usart_opt = 
	{
   		.baudrate     = BAUD_REMOTE,
	    .charlength   = 8,
	    .paritytype   = USART_NO_PARITY,
	    .stopbits     = USART_1_STOPBIT,
	    .channelmode  = USART_NORMAL_CHMODE
	};
	static const gpio_map_t USART_GPIO_MAP = 
	{	
   		{	AVR32_USART1_RXD_0_1_PIN, 
			AVR32_USART1_RXD_0_1_FUNCTION	
		},
		{	
			AVR32_USART1_TXD_0_1_PIN, 
			AVR32_USART1_TXD_0_1_FUNCTION	
		}
	};
	
	for (int32_t i = 0; i < 16; i++) 
	{
		sat.channels[i] = 0;
		channel_center[i] = 0;
	}
	 
    // Assign GPIO pins to USART_0.
    gpio_enable_module(	USART_GPIO_MAP,
                     	sizeof(USART_GPIO_MAP) / sizeof(USART_GPIO_MAP[0]) );
	
    // Initialize the USART in RS232 mode.
    usart_init_rs232( (&REMOTE_UART), &usart_opt, sysclk_get_cpu_hz() );
	INTC_register_interrupt( (__int_handler) &spectrum_handler, AVR32_USART1_IRQ, AVR32_INTC_INT1 );
	REMOTE_UART.ier = AVR32_USART_IER_RXRDY_MASK;

	//initUART_RX(&sat.receiver,  &USARTC1, USART_RXCINTLVL_LO_gc, BSEL_SPEKTRUM);
	
	spektrum_satellite_switch_on();
}


int16_t spektrum_satellite_get_channel(uint8_t index) 
{
	return sat.channels[index];
}


int16_t spektrum_satellite_get_neutral(uint8_t index) 
{
	int16_t value = spektrum_satellite_get_channel(index) - channel_center[index];

	// clamp to dead zone
	if ( (value > -DEADZONE) && (value < DEADZONE) )
	{
		value=0;
	}

	return value;
}


void spektrum_satellite_calibrate_center(uint8_t index)
{
	channel_center[index] = spektrum_satellite_get_channel(index);
}


int8_t spektrum_satellite_check(void) 
{
	int8_t i;
	uint32_t now = time_keeper_get_time_ticks();
	uint32_t duration = now - sat.last_update;
	
	if (sat.valid == 0)
	{
		return - 2;
	}

	if (duration < 100000) 
	{
		return 1;
	} 
	else if (duration < 1500000) 
	{
		sat.channels[RC_ROLL] = 0;	
		sat.channels[RC_PITCH] = 0;	
		sat.channels[RC_YAW] = 0;	
		
		return -1; // brief drop out - hold pattern
	} 
	else 
	{
		sat.valid = 0;
		for (i = 1; i < 8; i++) 
		{
			sat.channels[i] = 0;			
		}

		sat.channels[RC_THROTTLE] = -1000;
		return -2; // fade - fail safe
	}
}

/* File:   main.c
   Author: Jason Ui, Jeremy Long
   Date:   26 Apri 2021
   Descr:  Wacky Racer Main Racer Code
*/

// you guys might want to make sure you save your stuff and log out. good luck with the race <3
#include <pio.h>
#include "delay.h"
#include "pwm.h"
#include "target.h"
#include "pacer.h"
#include "nrf24.h"
#include "adc.h"
#include "usb_serial.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "ledtape.h"
#include "ledbuffer.h"
#include "button.h"
#include "mcu_sleep.h"

////Define
// PWM
#define AIN1_PIO PA24_PIO
#define AIN2_PIO PA25_PIO
#define BIN1_PIO PA7_PIO
#define BIN2_PIO PA11_PIO 
//System
#define PWM_FREQ_HZ 20000
//Battery Sensor
#define BATTERY_VOLTAGE_ADC ADC_CHANNEL_4
//Led Tape
#define NUM_LEDS 26




////Variables & Functions

///--------------------------------------------------------------------///
//LED Variables & Functions

/* Define how fast ticks occur.  This must be faster than
   TICK_RATE_MIN.  */
enum {LOOP_POLL_RATE = 200};

/* Define LED flash rate in Hz.  */
enum {LED_FLASH_RATE = 2};
///--------------------------------------------------------------------///

///--------------------------------------------------------------------///
//PWM Variables & Functions
static const pwm_cfg_t pwm1_cfg =
{
    .pio = AIN1_PIO,
    .period = PWM_PERIOD_DIVISOR (PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_LOW,
    .stop_state = PIO_OUTPUT_LOW
};

static const pwm_cfg_t pwm2_cfg =
{
    .pio = AIN2_PIO,
    .period = PWM_PERIOD_DIVISOR (PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_LOW,
    .stop_state = PIO_OUTPUT_LOW
};

static const pwm_cfg_t pwm3_cfg =
{
    .pio = BIN1_PIO,
    .period = PWM_PERIOD_DIVISOR (PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_LOW,
    .stop_state = PIO_OUTPUT_LOW
};

static const pwm_cfg_t pwm4_cfg =
{
    .pio = BIN2_PIO,
    .period = PWM_PERIOD_DIVISOR (PWM_FREQ_HZ),
    .duty = PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0),
    .align = PWM_ALIGN_LEFT,
    .polarity = PWM_POLARITY_LOW,
    .stop_state = PIO_OUTPUT_LOW
};

///--------------------------------------------------------------------///

///--------------------------------------------------------------------///
// Radio Variables & Functions

static void panic(void)
{
    while (1) {
        pio_output_toggle(LED1_PIO);
        pio_output_toggle(LED2_PIO);
        //delay_ms(400);
    }
}

///--------------------------------------------------------------------//
//USB
static usb_serial_cfg_t usb_serial_cfg =
{
    .read_timeout_us = 1,
    .write_timeout_us = 1,
};
///--------------------------------------------------------------------///
//Battery Detector
static adc_t battery_sensor;

static int battery_sensor_init(void)
{
    adc_cfg_t bat = {
        .channel = BATTERY_VOLTAGE_ADC,
        .bits = 12,
        .trigger = ADC_TRIGGER_SW,
        .clock_speed_kHz = F_CPU / 4000,
    };

    battery_sensor = adc_init(&bat);

    return (battery_sensor == 0) ? -1 : 0;
}

static uint16_t battery_millivolts(void)
{
    adc_sample_t s;
    adc_read(battery_sensor, &s, sizeof(s));

    // 33k pull down & 47k pull up gives a scale factor or
    // 33 / (47 + 33) = 0.4125
    // 4096 (max ADC reading) * 0.4125 ~= 1690
    //return (uint16_t)((int)s) * 3300 / 1690;
    return (uint16_t)((int)s) * 3300 / 636.33;
}

///--------------------------------------------------------------------///

int
main (void)
{
    ///--------------------------------------------------------------------///
    //MCU (Main)
    
    ///--------------------------------------------------------------------///
    //LED (Main)
    uint8_t flash_ticks;
    uint8_t duty_cyle1 = 0;
    const char* forward_slow = "F-";
    const char* forward_med = "F";
    const char* forward_fast = "F+";
    const char* backward_slow = "B-";
    const char* backward_med = "B";
    const char* backward_fast = "B+";
    const char* left = "L";
    const char* right = "R";
    const char* stop = "S";
    const char* forw_right = "FR";
    const char* forw_left = "FL";
    const char* backw_left = "BL";
    const char* backw_right = "BR";

    /* Configure LED PIO as output.  */
    pio_config_set (LED1_PIO, PIO_OUTPUT_LOW);
    pio_config_set (LED2_PIO, PIO_OUTPUT_HIGH);
    pio_config_set (LED3_PIO, PIO_OUTPUT_HIGH);

    pio_config_set (RADIO_CHANNEL_1, PIO_INPUT);
    pio_config_set (RADIO_CHANNEL_2, PIO_INPUT);

    pio_config_set(SLEEP_BUTTON, PIO_INPUT);
    pio_config_set(WAKE_BUTTON, PIO_INPUT);

    pacer_init (LOOP_POLL_RATE);
    flash_ticks = 0;
    ///--------------------------------------------------------------------///
    //LED Tape (Main)

    bool blue = false;
    int colorMode = 1; //1 = red, 2 = green, 3 = blue
    int LTape_count = 0;
    int current_Led = 0;
    int bmp_LED_count = 0;
    int red = 0;
    int bumper_i = 0;
    ledbuffer_t* leds = ledbuffer_init(LEDTAPE_PIO, NUM_LEDS);

    ///--------------------------------------------------------------------///
    //PWM (Main)
    pwm_t pwm1;
    pwm_t pwm2;
    pwm_t pwm3;
    pwm_t pwm4;

    pwm1 = pwm_init (&pwm1_cfg);
    pwm2 = pwm_init (&pwm2_cfg);
    pwm3 = pwm_init (&pwm3_cfg);
    pwm4 = pwm_init (&pwm4_cfg);

    pwm_channels_start (pwm_channel_mask (pwm1) | pwm_channel_mask (pwm2));
    pwm_channels_start (pwm_channel_mask (pwm3) | pwm_channel_mask (pwm4));

    pio_config_set (PWM_EN_PIO, PIO_OUTPUT_HIGH);

    pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
    pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
    pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
    pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
    ///--------------------------------------------------------------------///
    //Bumper (Main)

    int bumped = 0; 
    int bumper_count = 0;

    // Set up the buttons--------------------------------------------------------
    button_cfg_t bmp_cfg = {.pio = Bumper_PIO};
    button_t bmp_btn = button_init(&bmp_cfg);

    button_cfg_t sleep_cfg = {.pio = SLEEP_BUTTON};
    button_t sleep_btn = button_init(&sleep_cfg);

    button_cfg_t mcu_wakeup_cfg = {.pio = WAKE_BUTTON};
    button_t wake_btn =  button_init(&mcu_wakeup_cfg);
    
    pio_irq_config_t wake_pio_irq_config = PIO_IRQ_FALLING_EDGE;
    pio_irq_config_set (WAKE_BUTTON, wake_pio_irq_config);
    pio_irq_enable (WAKE_BUTTON);

    ///-------------------------------------------------------------
    //USB

    //USB Code (cancel out when not debugging)
    //Create non-blocking tty device for USB CDC connection.
     //usb_serial_init(NULL, "/dev/usb_tty");

     //freopen("/dev/usb_tty", "a", stdout);
    //freopen("/dev/usb_tty", "r", stdin);

    ///--------------------------------------------------------------------///
    //Radio (Main)
    spi_cfg_t nrf_spi = {
        .channel = 0,
        .clock_speed_kHz = 1000,
        .cs = RADIO_CS_PIO,
        .mode = SPI_MODE_0,
        .cs_mode = SPI_CS_MODE_FRAME,
        .bits = 8,
    };
    nrf24_t *nrf;
    spi_t spi;
    usb_cdc_t usb_cdc;

    spi = spi_init(&nrf_spi);
    nrf = nrf24_create(spi, RADIO_CE_PIO, RADIO_IRQ_PIO);


    mcu_sleep_cfg_t sleep_md = { .mode = MCU_SLEEP_MODE_WAIT};
    mcu_sleep_wakeup_cfg_t wake_md = { .pio = WAKE_BUTTON , .active_high = false };

    // initialize the NRF24 radio with its unique 5 byte address
    // if (!nrf24_begin(nrf, 4, 0x0987654321, 32))
    //     panic();
    // if (!nrf24_listen(nrf))
    //     panic();

    pacer_init (200);
    ///--------------------------------------------------------------------///
    //Battery

    battery_sensor_init();
    bool is_low_bat = false;
    // if (battery_sensor_init() < 0)
    //     panic();

    ///--------------------------------------------------------------------///
    //Debug
    
    if (!nrf)
    panic();

    if (!bmp_btn) {
        panic();
    }

    ///--------------------------------------------------------------------///
    //Channel Select

    long long current_Address = 0x6969696969;
    int current_Channel = 0;

    if (pio_input_get(RADIO_CHANNEL_1) == 0 && pio_input_get(RADIO_CHANNEL_2) == 0) {
        current_Address = 0x6969696969;
        current_Channel = 1;
        printf("channel 1");
        
    } 
    else if (pio_input_get(RADIO_CHANNEL_1) == 0 && pio_input_get(RADIO_CHANNEL_2) == 1) {
        current_Address = 0x2222222222;
        current_Channel = 2;
    } 
    else if (pio_input_get(RADIO_CHANNEL_1) == 1 && pio_input_get(RADIO_CHANNEL_2) == 0) {
        current_Address = 0x3333333333;
        current_Channel = 3;
    } 
    else if (pio_input_get(RADIO_CHANNEL_1) == 1 && pio_input_get(RADIO_CHANNEL_2) == 1) {
        current_Address = 0x4444444444;
        current_Channel = 4;
    }

    if (!nrf24_begin(nrf, current_Channel, current_Address, 32)) {
        panic(); 
    }   

    if (!nrf24_listen(nrf)) {
        panic();
    }



    while (1)
    {
        ///------------------------------------------------------------///
        //Clock (While Loop)
        /* Wait until next clock tick.  */
        pacer_wait ();
        flash_ticks++;

        ///------------------------------------------------------------///
        //Channel (While Loop)
        if (button_poll(sleep_btn) == BUTTON_STATE_DOWN) {//-------------------------------------------SLEEP/WAKEUP BUTTON-----------------------------------------
             ledbuffer_clear(leds);
            ledbuffer_write (leds);
            mcu_sleep(&sleep_md);
        }

        if (button_poll(wake_btn) == BUTTON_STATE_DOWN) {
             mcu_sleep_wakeup_set (&wake_md); 
        }
        
        //-------------------------------------------------------------------------------------------------------CHANNEL SELECT--------------------------------
        if (pio_input_get(RADIO_CHANNEL_1) == 0 && pio_input_get(RADIO_CHANNEL_2) == 0) {
        current_Address = 0x6969696969;
        current_Channel = 1;
        printf("Channel 1\n");
        
        } 
        else if (pio_input_get(RADIO_CHANNEL_1) == 0 && pio_input_get(RADIO_CHANNEL_2) == 1) {
            current_Address = 0x2222222222;
            current_Channel = 2;
            printf("Channel 2\n");
        } 
        else if (pio_input_get(RADIO_CHANNEL_1) == 1 && pio_input_get(RADIO_CHANNEL_2) == 0) {
            current_Address = 0x3333333333;
            current_Channel = 3;
            printf("Channel 3\n");
        } 
        else if (pio_input_get(RADIO_CHANNEL_1) == 1 && pio_input_get(RADIO_CHANNEL_2) == 1) {
            current_Address = 0x4444444444;
            current_Channel = 4;
            printf("Channel 4\n");
        }

        if (!nrf24_begin(nrf, current_Channel, current_Address, 32)) {
            panic(); 
        }   

        if (!nrf24_listen(nrf)) {
            panic();
        }

        ///------------------------------------------------------------///
        //Bumper-------------------------------------------------------------------------BUMPER----------------------------------------

        if (button_poll(bmp_btn) == BUTTON_STATE_DOWN) {
            printf("board bumped\n");
            bumped = 1; 
        }

        while (bumped == 1) {
            printf("board bumped is at %d\n",bumper_count);
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            // LED's
            if (bmp_LED_count++ == 10000) {
                for (bumper_i = 0; bumper_i < NUM_LEDS; bumper_i++) {
                        ledbuffer_set(leds, bumper_i, 255, 0, 0);
                }
                ledbuffer_write (leds);
            }
            else {
                ledbuffer_write (leds);
                for (bumper_i = 0; bumper_i < NUM_LEDS; bumper_i++) {
                ledbuffer_set(leds, bumper_i, 0, 0, 0);
                }
                ledbuffer_write (leds);
            }
            bmp_LED_count = 0;
            //turn everything off
            bumper_count++;
            if(bumper_count >= 35700) { //time is ~~1.4x bumper count  35700
                bumper_count = 0;
                bumped = 0;
            }
        }

        ///------------------------------------------------------------///
        //Radio (While Loop)----------------------------------------------------------------------RADIO--------------------------------------------
        

        //Read Radio
        char buffer[32];
        
        if (nrf24_read(nrf, buffer, sizeof(buffer))) {
            printf("Buffer is %s\n", buffer);
            pio_output_toggle (LED1_PIO);
            pio_output_toggle (LED2_PIO);
        } 
            

        ///------------------------------------------------------------///
        //LED TAPE-------------------------------------------------------------------------------------LED TAPE------------------------------------------------

        // if (LTape_count++ == NUM_LEDS) {
        //     // wait for a revolution
        //     ledbuffer_clear(leds);
        //     if (blue) {
        //         ledbuffer_set(leds, 0, 0, 0, 255);
        //         ledbuffer_set(leds, NUM_LEDS / 2, 0, 0, 255);
        //     } else {
        //         ledbuffer_set(leds, 0, 255, 0, 0);
        //         ledbuffer_set(leds, NUM_LEDS / 2, 255, 0, 0);
        //     }
        //     blue = !blue;
        //     LTape_count = 0;
        // }

        if(!is_low_bat) {
            ledbuffer_clear(leds);
            if (colorMode == 1) {
                ledbuffer_set(leds, current_Led, 255, 0, 0);
            } else if (colorMode == 2) {
                ledbuffer_set(leds, current_Led, 0, 255, 0);
            } else if (colorMode == 3) {
                ledbuffer_set(leds, current_Led, 0, 0, 255);
            }
            colorMode++;
            current_Led++;

            if (colorMode > 3) {
                colorMode = 0;
            }

            if (current_Led > NUM_LEDS) {
                current_Led = 0;
            }

            ledbuffer_write (leds);
            ledbuffer_advance (leds, 1); 
        }
        


        ///------------------------------------------------------------///
        // IF FUNCTION---------------------------------------------------------------------------LOW BATTERY INDICATOR---------------------------------------------
        if (flash_ticks >= LOOP_POLL_RATE / (LED_FLASH_RATE * 2))
        {
            flash_ticks = 0; 
        }

        //Flash the lights if we ever go to low battery state
        if (battery_millivolts() < 5000) {
            ledbuffer_clear(leds);
            ledbuffer_write (leds);
            printf("Battery is %d\n", battery_millivolts());
            pio_output_toggle(LED3_PIO);
            is_low_bat = true;
        } else {
            is_low_bat = false;
        }   
                //-----------------------------------------------------------------------------------MOTOR CONTROL-------------------------------------------------------
        if (strcmp(buffer, forward_slow) == 0 && bumped == 0) {
            printf("motor forward\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

            if (strcmp(buffer, forward_med) == 0 && bumped == 0 && !is_low_bat) {
            printf("motor forward\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 70));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 70));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

            if (strcmp(buffer, forward_fast) == 0 && bumped == 0 && !is_low_bat) {
            printf("motor forward\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 100));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 100));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 
        
        if (strcmp(buffer, backward_slow) == 0 && bumped == 0) {
            printf("motor backwards\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
        } 

            if (strcmp(buffer, backward_med) == 0 && bumped == 0 && !is_low_bat) {
            printf("motor backwards\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 70));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 70));
        } 

            if (strcmp(buffer, backward_fast) == 0 && bumped == 0 && !is_low_bat) {
            printf("motor backwards\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 100));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 100));
        } 

        if (strcmp(buffer, left) == 0 && bumped == 0) {
            printf("motor left\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 80));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

        if (strcmp(buffer, right) == 0 && bumped == 0) {
            printf("motor right\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 80));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

        if (strcmp(buffer, forw_left) == 0 && bumped == 0) {
            printf("motor forw left\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 30));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

        if (strcmp(buffer, forw_right) == 0 && bumped == 0) {
            printf("motor forw right\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 30));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        } 

        if (strcmp(buffer, backw_left) == 0 && bumped == 0) {
            printf("motor backwards left\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 30));
        } 

        if (strcmp(buffer, backw_right) == 0 && bumped == 0) {
            printf("motor backwards right\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 30));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 50));
        } 

        if (strcmp(buffer, stop) == 0) {
            printf("board flat\n");
            pwm_duty_set (pwm1, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm2, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm3, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
            pwm_duty_set (pwm4, PWM_DUTY_DIVISOR (PWM_FREQ_HZ, 0));
        }

        ///------------------------------------------------------------///

    }
}


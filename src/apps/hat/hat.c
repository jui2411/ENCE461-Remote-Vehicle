/* File:   imu_test1.c
   Author: B Mitchell, UCECE
   Date:   15/4/2021
   Descr:  Read from an MP9250 IMU and write its output to the USB serial.
*/
#include <pio.h>
#include <fcntl.h>
#include "target.h"
#include "pacer.h"
#include "usb_serial.h"
#include "mpu9250.h"
#include "nrf24.h"
#include "stdio.h"
#include "delay.h"
#include "adc.h"
#include "piezo_beep.h"
#include "config.h"
#include "piezo.h"
#include <math.h>
#include "button.h"
#include "mcu_sleep.h"
#include "ledbuffer.h"
#include "stdlib.h"

#define SLEEP PB3_PIO
#define JOYSTICK_BUTTON PA2_PIO
#define RADIO_CHANNEL_1 PA31_PIO
#define RADIO_CHANNEL_2 PA30_PIO
#define JOYSTICK_ADC_X ADC_CHANNEL_3
#define JOYSTICK_ADC_Y ADC_CHANNEL_0
#define BATTERY_VOLTAGE_ADC ADC_CHANNEL_2
#define LEDTAPE_PIO PA9_PIO
#define NUM_LEDS 26

#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978
#define REST      0

/* Define how fast ticks occur.  This must be faster than
   TICK_RATE_MIN.  */
enum {LOOP_POLL_RATE = 200};

/* Define LED flash rate in Hz.  */
enum {LED_FLASH_RATE = 2};

static usb_serial_cfg_t usb_serial_cfg =
{
    .read_timeout_us = 1,
    .write_timeout_us = 1,
};

static adc_t joystick_x;
static adc_t joystick_y;
static adc_t battery_sensor;

static twi_cfg_t mpu_twi_cfg =
{
    .channel = TWI_CHANNEL_0,
    .period = TWI_PERIOD_DIVISOR(100000), // 100 kHz
    .slave_addr = 0
};

static void panic(void)
{
    while (1) {
        pio_output_toggle(LED1_PIO);
        pio_output_toggle(LED2_PIO);
        // delay_ms(400);
    }
}

static int joystick_x_init(void)
{
    adc_cfg_t joystick_x_axis = {
        .channel = JOYSTICK_ADC_X,
        .bits = 12,
        .trigger = ADC_TRIGGER_SW,
        .clock_speed_kHz = F_CPU / 4000,
    };

    joystick_x = adc_init(&joystick_x_axis);

    return (joystick_x == 0) ? -1 : 0;
}

static uint16_t joystick_x_read(void)
{
    adc_sample_t s;
    adc_read(joystick_x, &s, sizeof(s));

    // 33k pull down & 47k pull up gives a scale factor or
    // 33 / (47 + 33) = 0.4125
    // 4096 (max ADC reading) * 0.4125 ~= 1690
    return (uint16_t)((int)s) * 3300 / 636.33;
}

static int joystick_y_init(void)
{
    adc_cfg_t joystick_y_axis = {
        .channel = JOYSTICK_ADC_Y,
        .bits = 12,
        .trigger = ADC_TRIGGER_SW,
        .clock_speed_kHz = F_CPU / 4000,
    };

    joystick_y = adc_init(&joystick_y_axis);

    return (joystick_y == 0) ? -1 : 0;
}

static uint16_t joystick_y_read(void)
{
    adc_sample_t s;
    adc_read(joystick_y, &s, sizeof(s));

    // 33k pull down & 47k pull up gives a scale factor or
    // 33 / (47 + 33) = 0.4125
    // 4096 (max ADC reading) * 0.4125 ~= 1690
    return (uint16_t)((int)s) * 3300 / 636.33;
}

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
    return (uint16_t)((int)s) * 3300 / 636.33;
}


int
main (void)
{

    int tempo = 200;
    int melody[] = {


    // Nokia Ringtone 
    // Score available at https://musescore.com/user/29944637/scores/5266155
    
    NOTE_E5, 8, NOTE_D5, 8, NOTE_FS4, 4, NOTE_GS4, 4, 
    NOTE_CS5, 8, NOTE_B4, 8, NOTE_D4, 4, NOTE_E4, 4, 
    NOTE_B4, 8, NOTE_A4, 8, NOTE_CS4, 4, NOTE_E4, 4,
    NOTE_A4, 2, 
    };

    // double frequency[] = {2, 0.5, 2, 0.5, 1, 0.5, 1, 0.5, 2, 0.5, 2, 0.5, 1, 0.5, 1, 0.5, 2, 0.5, 1, 
    // 0.5, 1, 0.5, 1, 0.5, 1, 0.5, 1, 0.5, 1, 0.5, 1, 0.5, 1, 0.5,
    // };

    double frequency[] = {0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 
    0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1, 0.5, 0.1,
    };
    int notes = sizeof(melody) / sizeof(melody[0]) / 2;
    int wholenote = (60000 * 4) / tempo;
    int divider = 0;
    uint16_t noteDuration = 0;

    ///--------------------------------------------------------------------///
    //LED Tape (Main)

    bool blue = false;
    int colorMode = 1; //1 = red, 2 = green, 3 = blue
    int LTape_count = 0;
    int current_Led = 0;

    ledbuffer_t* leds = ledbuffer_init(LEDTAPE_PIO, NUM_LEDS);

    uint8_t flash_ticks;
    //Create non-blocking tty device for USB CDC connection.
    // usb_serial_init (&usb_serial_cfg, "/dev/usb_tty");

    // freopen ("/dev/usb_tty", "a", stdout);
    // freopen ("/dev/usb_tty", "r", stdin);

    // Initialise the TWI (I2C) bus for the MPU
    twi_t twi_mpu = twi_init (&mpu_twi_cfg);
    // Initialise the MPU9250 IMU
    mpu_t* mpu = mpu9250_create (twi_mpu, MPU_ADDRESS);

    //piezo_beep_short(Buzzer);

    uint8_t count = 0;
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

    piezo_cfg_t buzzer = {
        .pio = PA8_PIO //buzzer pin
    };

    // mcu_sleep_wakeup_cfg_t wakeup = {
    //     .pio = PA2_PIO,
    //     .active_high = true
    // };

    mcu_sleep_cfg_t mode = {
        .mode = MCU_SLEEP_MODE_WAIT
    };

    /* Configure LED PIO as output.  */
    pio_config_set (LED1_PIO, PIO_OUTPUT_HIGH);
    pio_config_set (LED2_PIO, PIO_OUTPUT_LOW);
    pio_config_set (RADIO_CHANNEL_1, PIO_INPUT);
    pio_config_set (RADIO_CHANNEL_2, PIO_INPUT);
    // pio_config_set (JOYSTICK_BUTTON, PIO_PULLDOWN);

    button_cfg_t sleep = {.pio = SLEEP};
    button_t sleep_btn = button_init(&sleep);

    button_cfg_t sound = {.pio = JOYSTICK_BUTTON};
    button_t sound_btn = button_init(&sound);

    if (!sleep_btn)
        panic();

    if (!sound_btn)
        panic();

    // mcu_sleep_wakeup_set(&wakeup);

    pacer_init (10);
    
    flash_ticks = 0;

#ifdef RADIO_PWR_EN
    pio_config_set(RADIO_PWR_EN, PIO_OUTPUT_HIGH);
#endif

    if (joystick_y_init())
        panic();

    if (joystick_x_init())
        panic();

    if (battery_sensor_init() < 0)
        panic();

    piezo_init(&buzzer);

    // mcu_sleep_wakeup_set(&wakeup);


    spi = spi_init(&nrf_spi);
    nrf = nrf24_create(spi, RADIO_CE_PIO, RADIO_IRQ_PIO);
    if (!nrf)
        panic();
    
    long long Current_Address = 0x6969696969;
    int Current_Channel = 0;

    if ((pio_input_get(RADIO_CHANNEL_1) == 0) && (pio_input_get(RADIO_CHANNEL_2) == 0)) {
        Current_Address = 0x6969696969;
        Current_Channel = 1;
        printf("Channel 1\n");
    }
    // initialize the NRF24 radio with its unique 5 byte address
    if ((pio_input_get(RADIO_CHANNEL_1) == 1) && (pio_input_get(RADIO_CHANNEL_2) == 0)) {
        Current_Address = 0x3333333333;
        Current_Channel = 3;
        printf("Channel 3\n");
    }

    if ((pio_input_get(RADIO_CHANNEL_2) == 1) && (pio_input_get(RADIO_CHANNEL_1) == 0)) {
        Current_Address = 0x2222222222;
        Current_Channel = 2;
        printf("Channel 2\n");
    }

    if ((pio_input_get(RADIO_CHANNEL_2) == 1) && (pio_input_get(RADIO_CHANNEL_1) == 1)) {
        Current_Address = 0x4444444444;
        Current_Channel = 4;
        printf("Channel 4\n");
    }

    if (!nrf24_begin(nrf, Current_Channel, Current_Address, 32)) {
            
            panic();
    }

    while (1)
    {
        /* Wait until next clock tick.  */
        char buffer[32];
        pacer_wait ();
        //printf("im running");
        // pio_output_set(LED1_PIO, 0);

        if ((pio_input_get(RADIO_CHANNEL_1) == 0) && (pio_input_get(RADIO_CHANNEL_2) == 0)) {
            Current_Address = 0x6969696969;
            Current_Channel = 1;
            printf("Channel 1\n");
        }
        // initialize the NRF24 radio with its unique 5 byte address
        if ((pio_input_get(RADIO_CHANNEL_1) == 1) && (pio_input_get(RADIO_CHANNEL_2) == 0)) {
            Current_Address = 0x3333333333;
            Current_Channel = 3;
            printf("Channel 3\n");
        }

        if ((pio_input_get(RADIO_CHANNEL_2) == 1) && (pio_input_get(RADIO_CHANNEL_1) == 0)) {
            Current_Address = 0x2222222222;
            Current_Channel = 2;
            printf("Channel 2\n");
        }

        if ((pio_input_get(RADIO_CHANNEL_2) == 1) && (pio_input_get(RADIO_CHANNEL_1) == 1)) {
            Current_Address = 0x4444444444;
            Current_Channel = 4;
            printf("Channel 4\n");
        }

        if (!nrf24_begin(nrf, Current_Channel, Current_Address, 32)) {
                
                panic();
        }    

        if (button_poll(sleep_btn) == BUTTON_STATE_DOWN) {
            for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

                // calculates the duration of each note
                divider = melody[thisNote + 1];
                if (divider > 0) {
                // regular note, just proceed
                noteDuration = (wholenote) / divider;
                } else if (divider < 0) {
                // dotted notes are represented with negative durations!!
                noteDuration = (wholenote) / abs(divider);
                noteDuration *= 1.5; // increases the duration in half for dotted notes
                }

                // we only play the note for 90% of the duration, leaving 10% as a pause
                piezo_beep2(&buzzer, noteDuration, 0.5);

                // Wait for the specief duration before playing the next note.
                delay_ms(10);
            }
        }        

        if (mpu)
        {
            /* read in the accelerometer data */
            if (! mpu9250_is_imu_ready (mpu))
            {
                printf("Waiting for IMU to be ready...\n");
            }
            else
            {
                int16_t accel[3];
                if (mpu9250_read_accel (mpu, accel))
                {
                    
                    double accel_x = accel[0];
                    double accel_y = accel[1];
                    double accel_z = accel[2];
                    double xz = accel_x / accel_z;
                    double yz = accel_y / accel_z;
                    int theta = 100 * atan(xz);
                    int phi = 100 * atan(yz);
                    int angle = atan2(theta, phi);

                    int joystick_x_axis = joystick_x_read() / 1000;
                    int joystick_y_axis = joystick_y_read() / 1000;


                    if (theta >= 25 && theta <= 50 && phi > -25) {
                        sprintf(buffer, "F-"); // backwards
                    }
                    // else if (theta >= 35 && phi <= 0) {
                    //     sprintf(buffer, "B");
                    // }
                    else if (theta > 50 && phi > -25) {
                        sprintf(buffer, "F+");
                    }                                        
                    else if (theta < 0 && phi >= 25){
                        sprintf(buffer, "FR"); 
                    }
                    // else if (theta < -40 && phi > 0) {
                    //     sprintf(buffer, "F");
                    // }
                    else if (theta <= -25 && theta >= -50 && phi < 25) {
                        sprintf(buffer, "B-");
                    }
                    else if (theta < -50 && phi < 25) {
                        sprintf(buffer, "B+");
                    }                    
                    else if (theta < 0 && phi <= -25) {
                        sprintf(buffer, "FL"); // left
                    }
                    // else if (theta < -40 && phi <-30) {
                    //     sprintf(buffer, "FL");
                    // }
                    // else if (theta < -40 && phi > 20) {
                    //     sprintf(buffer, "FR");
                    // }
                    // else if (theta > 30 && phi < -30) {
                    //     sprintf(buffer, "BL");
                    // }
                    // else if (theta > 30 && phi > 30) {
                    //     sprintf(buffer, "BR");
                    // }                                                            
                    else {
                        sprintf(buffer, "S");
                    }
                    printf("%d,%d", theta, phi);

                    if (!((joystick_x_axis == 9 && joystick_y_axis == 9) || (joystick_x_axis == 10 && joystick_y_axis == 9))) {
                        double accel_x = 0;
                        double accel_y = 0;
                        double accel_z = 0;
                        if (joystick_x_axis > 9 && joystick_x_axis <= 12) {
                            sprintf(buffer, "F-"); // backwards
                        }
                        else if (joystick_x_axis > 12) {
                            sprintf(buffer, "F+");
                        }                        
                        // else if (joystick_x_axis >= 9 && joystick_y_axis > 9){
                        //     sprintf(buffer, "R"); 
                        // }
                        else if (joystick_x_axis < 9 && joystick_x_axis >= 5) {
                            sprintf(buffer, "B-");
                        }
                        else if (joystick_x_axis < 5) {
                            sprintf(buffer, "B+");
                        }                        
                        // else if (joystick_x_axis <= 9 && joystick_y_axis < 9) {
                        //     sprintf(buffer, "L"); // left
                        // }
                        else if (joystick_x_axis <= 9 && joystick_y_axis < 9) {
                            sprintf(buffer, "FL");
                        }
                        else if (joystick_x_axis >= 9 && joystick_y_axis > 10) {
                            sprintf(buffer, "FR");
                        }
                        // else if (joystick_x_axis > 13 && joystick_y_axis <= 9) {
                        //     sprintf(buffer, "BL");
                        // }
                        // else if (joystick_x_axis > 13 && joystick_y_axis > 9) {
                        //     sprintf(buffer, "BR");
                        // }            
                        else {
                            sprintf(buffer, "S");
                        }
                        // printf("%d,%d\n", joystick_x_axis, joystick_y_axis);                        
                    }
                    //sprintf(buffer, "%d,%d", theta, phi);                    
                    // printf("x: %d  y: %d  z: %d\n", theta, pi, accel[2]);
                    if (!nrf24_write(nrf, buffer, sizeof(buffer))){
                        pio_output_set(LED2_PIO, 0);
                    }
                    else {
                        pio_output_toggle(LED2_PIO);
                    }
                }
                else
                {
                    printf("ERROR: failed to read acceleration\n");
                }
            }
        }
        else
        {
            printf("ERROR: can't find MPU9250!\n");
        }

        if (battery_millivolts() < 5000) {
            pio_output_toggle(LED1_PIO);
            // delay_ms(200);
        } else {
            pio_output_set(LED1_PIO, 1);
        }

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
        
        fflush(stdout);
    }
}

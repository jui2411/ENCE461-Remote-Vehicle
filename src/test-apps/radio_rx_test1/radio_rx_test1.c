/* File:   radio_rx_test1.c
   Author: M. P. Hayes, UCECE
   Date:   24 Feb 2018
   Descr:
*/
#include "nrf24.h"
#include "usb_serial.h"
#include "pio.h"
#include "delay.h"
#include "stdlib.h"
#include "string.h"
#include "pacer.h"
#include <stdio.h>
#include <stdlib.h>


static void panic(void)
{
    while (1) {
        pio_output_toggle(LED1_PIO);
        pio_output_toggle(LED2_PIO);
        delay_ms(400);
    }
}

int main (void)
{

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

    /* Configure LED PIO as output.  */
    pio_config_set(LED1_PIO, PIO_OUTPUT_LOW);
    pio_config_set(LED2_PIO, PIO_OUTPUT_LOW);

    // Create non-blocking tty device for USB CDC connection.
    usb_serial_init(NULL, "/dev/usb_tty");

    freopen("/dev/usb_tty", "a", stdout);
    freopen("/dev/usb_tty", "r", stdin);

    spi = spi_init(&nrf_spi);
    nrf = nrf24_create(spi, RADIO_CE_PIO, RADIO_IRQ_PIO);
    if (!nrf)
        panic();

    // initialize the NRF24 radio with its unique 5 byte address
    if (!nrf24_begin(nrf, 2, 0x1234567890, 32))
        panic();
    if (!nrf24_listen(nrf))
        panic();
    
    pacer_init (10);
    int i = 0;

    while (1)
    {
        
        pacer_wait();

        char buffer[32];
        //char data_array[1];
        
        if (nrf24_read(nrf, buffer, sizeof(buffer))) {
            printf("Buffer is %s\n", buffer);
            pio_output_toggle(LED2_PIO);
            pio_output_toggle(LED1_PIO);

            // //using strtok() function to seperate received character------------------------
            // const char s[2] = ",";
            // char *token;

            // /* get the first token */
            // token = strtok(buffer, s);

            // /* walk through other tokens */
            // while ( token != NULL ) {
            //     printf( "Token1 is %s\n", token );
            //     data_array[i] = token;
            //     token = strtok(NULL, s);
            //     //printf( "Token2 is %s\n", token );
            //     i = i + 1;
            //     if (i == 2){
            //         i = 0;
            //     }
            // }

            // printf("i is %d\n" , i);
            // int x = atoi(data_array[0]);
            // int y = atoi(data_array[1]);
            // printf("x is %d y is %d\n" , x, y);
            // if ( ( x < 0) && (y < 0) ) { //left, down
            //     printf("tilted left down");
            // }
            // else {
            //     printf("not left down");
            // }
            

        //printf( " %s\n", data_array );

        }
        
        else {
            pio_output_low(LED1_PIO);
        }
    }
}

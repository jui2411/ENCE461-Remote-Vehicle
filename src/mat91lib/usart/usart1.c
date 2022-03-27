/** @file   usart1.h
    @author Michael Hayes
    @date   10 March 2005
    @brief  Routines for interfacing with the usart1 on an AT91 ARM
*/

#include "mcu.h"
#include "pio.h"
#include "usart1.h"
#include "usart1_defs.h"

/* Temporary hack.  */
#undef USART1_USE_HANDSHAKING


/* Define in target.h to use hardware flow control.   */
#ifdef USART1_USE_HANDSHAKING
#define USART1_MODE US_MR_USART_MODE_HW_HANDSHAKING
#else
#define USART1_MODE US_MR_USART_MODE_NORMAL
#endif


void
usart1_baud_divisor_set (uint16_t baud_divisor)
{
    USART1_BAUD_DIVISOR_SET (baud_divisor);
}


int
usart1_init (uint16_t baud_divisor)
{
    /* Disable interrupts.  */
    USART1->US_IDR = ~0;

    /* Enable RxD1 and TxD1 pins and disable pullups.  */
    pio_config_set (RXD1_PIO, RXD1_PERIPH);
    pio_config_set (TXD1_PIO, TXD1_PERIPH);

#ifdef USART1_USE_HANDSHAKING
    pio_config_set (RTS1_PIO, RTS1_PERIPH);
    pio_config_set (CTS1_PIO, CTS1_PERIPH);
#endif
    
    /* Enable USART1 clock.  */
    mcu_pmc_enable (ID_USART1);
    
    /* Reset and disable receiver and transmitter.  */
    USART1->US_CR = US_CR_RSTRX | US_CR_RSTTX          
        | US_CR_RXDIS | US_CR_TXDIS;           

    /* Set normal mode, clock = MCK, 8-bit data, no parity, 1 stop bit.  */
    USART1->US_MR = USART1_MODE
        | US_MR_CHRL_8_BIT | US_MR_PAR_NO | US_MR_NBSTOP_1_BIT;

    usart1_baud_divisor_set (baud_divisor);

    /* Enable receiver and transmitter.  */
    USART1->US_CR = US_CR_RXEN | US_CR_TXEN; 
    
    return 1;
}


void
usart1_shutdown (void)
{
    /* Disable RxD1 and TxD1 pins.  */
    pio_config_set (RXD1_PIO, PIO_PULLUP);
    pio_config_set (TXD1_PIO, PIO_OUTPUT_LOW);

    /* Disable USART1 clock.  */
    mcu_pmc_disable (ID_USART1);
    
    /* Reset and disable receiver and transmitter.  */
    USART1->US_CR = US_CR_RSTRX | US_CR_RSTTX          
        | US_CR_RXDIS | US_CR_TXDIS;           
}


/* Return non-zero if there is a character ready to be read.  */
bool
usart1_read_ready_p (void)
{
#if HOSTED
    return 1;
#else
    return USART1_READ_READY_P ();
#endif
}


/* Return non-zero if a character can be written without blocking.  */
bool
usart1_write_ready_p (void)
{
    return USART1_WRITE_READY_P ();
}


/* Return non-zero if transmitter finished.  */
bool
usart1_write_finished_p (void)
{
    return USART1_WRITE_FINISHED_P ();
}


/* Write character to USART1.  This blocks.  */
int
usart1_putc (char ch)
{
    if (ch == '\n')
        usart1_putc ('\r');

    USART1_PUTC (ch);
    return ch;
}


/* Read character from USART1.  This does not block.  */
int
usart1_getc (void)
{
    if (! USART1_READ_READY_P ())
    {
        errno = EAGAIN;
        return -1;
    }

    return USART1_READ ();
}


/* Write string to USART1.  This blocks until the string is written.  */
int
usart1_puts (const char *str)
{
    while (*str)
    {
        if (usart1_putc (*str++) < 0)
            return -1;
    }
    return 1;
}


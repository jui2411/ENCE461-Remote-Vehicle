/** @file   sys.c
    @author M. P. Hayes, UCECE
    @date   04 June 2007
    @brief  System routines for SAM4S processors
*/

#include "config.h"
#include "sys.h"
#include "cpu.h"
#include "irq.h"

/* TODO: CHECKME  */
#define SYS_FLASH_SPEED 30e6

#ifndef SYS_FLASH_READ_CYCLES 
#define SYS_FLASH_READ_CYCLES 2
#endif


#ifdef CPU_PLL_DIV
#define SYS_PLL_DIV CPU_PLL_DIV
#endif

#ifdef CPU_PLL_MUL
#define SYS_PLL_MUL CPU_PLL_MUL
#endif

#if SYS_PLL_MUL > 80
#error SYS_PLL_MUL must be less than 80
#endif


#define F_PLL_IN = F_XTAL / SYS_PLL_DIV
#define F_PLL_OUT = F_PLL_IN * SYS_PLL_MUL

/* The PLL input frequency needs to be between 3 and 32 MHz.  This is
   the frequency after the divider and before the frequency
   multiplier.  */
#define F_PLL_IN_MIN    3000000
#define F_PLL_IN_MAX   32000000


/* The PLL output frequency needs to be between 80 and 240 MHz.   */
#define F_PLL_OUT_MIN  80000000
#define F_PLL_OUT_MAX 240000000


/* Internal slow clock frequency.  */
#define F_SLCK 32678

#define SYS_OS_DELAY 1.5e-3
#define SYS_OS_COUNT ((uint16_t) ((SYS_OS_DELAY * F_SLCK + 7)) / 8)

#define SYS_PLL_COUNT 0x3fu

/* TODO: the requried number of slow clock cycles divided by 8.  */
#define SYS_MAINCK_COUNT 100

#define SYS_USB_LOG2_DIV 0

/* The PLL frequency is given by (F_XTAL * SYS_PLL_MUL) / SYS_PLL_DIV.
   This is then divided by the prescaler (assumed 2) for MCK.  */



/* The AT91 Flash is single plane so it is not possible
   to write to it while executing code out of it.  */

/** Initialise flash memory controller.  */
static void
sys_flash_init (void)
{
#if 0
    /* TODO  */
    switch (SYS_FLASH_READ_CYCLES)
    {
    case 1:
        /* Set 0 flash wait states for reading, 1 for writing.  */
        EEFC->MC_FMR = MC_FWS_0FWS;
        break;

    case 2:
        /* Set 1 flash wait state for reading, 2 for writing.  */
        EEFC->MC_FMR = MC_FWS_1FWS;
        break;

    case 3:
        /* Set 2 flash wait states for reading, 3 for writing.  */
        EEFC->MC_FMR = MC_FWS_2FWS;
        break;

    default:
        /* Set 3 flash wait states for reading, 4 for writing.  */
        EEFC->MC_FMR = MC_FWS_3FWS;
        break;
    }

    /* Set number of MCK cycles per microsecond for the Flash
       microsecond cycle number (FMCN) field of the Flash mode
       register (FMR).  */
    BITS_INSERT (EEFC->MC_FMR, (uint16_t) (F_CPU / 1e6), 16, 23);
#endif
}


static void
sys_xtal_mainck_start (void)
{
    PMC->CKGR_MOR = (PMC->CKGR_MOR & ~CKGR_MOR_MOSCXTBY) |
        CKGR_MOR_KEY (0x37) | CKGR_MOR_MOSCXTEN |
        CKGR_MOR_MOSCXTST (SYS_MAINCK_COUNT);
    
    /* Wait for the xtal oscillator to stabilize.  */
    while (! (PMC->PMC_SR & PMC_SR_MOSCXTS))
        continue;
    
    PMC->CKGR_MOR |= CKGR_MOR_KEY (0x37) | CKGR_MOR_MOSCSEL;

    /* Could check if xtal oscillator fails to start; say if xtal
       not connected.  */
}


static bool
sys_mck_ready_wait (void)
{
    int timeout;

    /* Whenever PMC_MCKR is written then PMC_SR_MCKRDY is cleared; it
       gets set when the mck is established.  */

    for (timeout = 1000; timeout && ! (PMC->PMC_SR & PMC_SR_MCKRDY); timeout--)
        continue;
 
    return timeout != 0;
}


/** Set up the main clock (MAINCK), PLL clock, and master clock (MCK).   */
static int
sys_clock_init (void)
{
    /* To minimize the power required to start up the system, the main
       oscillator is disabled after reset and slow clock is
       selected. 

       There are four clock sources: SLCK (the 32 kHz internal RC
       oscillator or 32 kHz external crystal slow clock), MAINCK (the
       external 3-20 MHz crystal or internal 4/8/12 MHz internal fast
       RC oscillator main clock), PLLACK, and PLLBCK.  The two PLL
       clocks are the outputs of the the phase locked loop driven by
       MAINCK).  One of these four clock sources can be fed to a
       prescaler (with divisors 2^0 ... 2^6) to drive MCK (master
       clock).
       
       The main oscillator (external crystal) can range from 3--20 MHz.
       The PLL frequency can range from 80--240 MHz.

       Here we assume that an external crystal is used for the MAINCK
       and this is multipled by PLLA to drive MCK.

       Initially MCK is driven from the 4 MHZ internal fast RC oscillator.
    */

    /* Start xtal oscillator and select as MAINCK.  */
    sys_xtal_mainck_start ();
    
    /* Select MAINCK for MCK (this should already be selected).  */
    PMC->PMC_MCKR = (PMC->PMC_MCKR & (~PMC_MCKR_CSS_Msk)) | PMC_MCKR_CSS_MAIN_CLK;
    if (!sys_mck_ready_wait ())
        return 0;

    /* Set prescaler to 2.  */
    PMC->PMC_MCKR = (PMC->PMC_MCKR & (~PMC_MCKR_PRES_Msk)) | PMC_MCKR_PRES_CLK_2;
    if (!sys_mck_ready_wait ())
        return 0;

    /* Could disable internal fast RC oscillator here.  */


    /* Disable PLLA if it is running and reset fields.  */
    PMC->CKGR_PLLAR = CKGR_PLLAR_ONE | CKGR_PLLAR_MULA (0);

    /* Configure and start PLLA.  The PLL start delay is SYS_PLL_COUNT SLCK cycles.  
       Note, PLLA (but not PLBB) needs the mysterious bit CKGR_PLLAR_ONE set.  */
    PMC->CKGR_PLLAR = CKGR_PLLAR_MULA (SYS_PLL_MUL - 1) | CKGR_PLLAR_DIVA (SYS_PLL_DIV) 
        | CKGR_PLLAR_PLLACOUNT (SYS_PLL_COUNT) | CKGR_PLLAR_ONE;

    /* Wait for PLLA to start up.  */
    while (! (PMC->PMC_SR & PMC_SR_LOCKA))
        continue;

    /* Switch to PLLA_CLCK for MCK.  */
    PMC->PMC_MCKR = (PMC->PMC_MCKR & (~PMC_MCKR_CSS_Msk)) | PMC_MCKR_CSS_PLLA_CLK;
    if (!sys_mck_ready_wait ())
        return 0;
    
    return 1;
}


extern void _irq_unexpected_handler (void);

extern void _irq_spurious_handler (void);


/* Enable NRST pin.  */
void
sys_reset_enable (void)
{
    RSTC->RSTC_MR |= RSTC_MR_URSTEN | (0xa5 << 24);
}


/* Disable NRST pin.  */
void
sys_reset_disable (void)
{
    /* Disable NRST pin.  */
    RSTC->RSTC_MR =
        (RSTC->RSTC_MR & ~RSTC_MR_URSTEN) | (0xa5 << 24);
}


uint8_t
cpu_reset_type_get (void)
{
    return (RSTC->RSTC_SR >> 8) & 0x07;
}


/** Disable watchdog.  */
void
sys_watchdog_disable (void)
{
    WDT->WDT_MR = WDT_MR_WDDIS;
}


/** Initialise flash, disable watchdog, set up clocks.  This and any
    functions it calls must be for the C runtime startup to
    work.  */
void 
sys_init (void)
{
    irq_id_t id;
    int i;

    /* Disable all interrupts to be sure when debugging.  */
    for (i = 0; i < 8; i++)
        NVIC->ICER[i] = ~0;

    sys_reset_enable ();

    /* Reduce the number of wait states for the flash memory.  */
    sys_flash_init ();

    sys_watchdog_disable ();

    sys_clock_init ();

#if 0
    /* Enable protect mode.  */
    AIC->AIC_DCR |= AIC_DCR_PROT;
    /* Use MATRIX_WPMR ?  */
#endif

#if 0
    AIC->AIC_SPU = (int) _irq_spurious_handler;

    for (id = IRQ_ID_MIN; id <= IRQ_ID_MAX; id++) 
        irq_vector_set (id, _irq_unexpected_handler);
#endif
}



void
sys_reset (void)
{
    /* Reset processor and peripherals.  */
    RSTC->RSTC_CR = RSTC_CR_PROCRST | RSTC_CR_PERRST 
        | (0xa5 << 24);
}


/* Place this function in SRAM to avoid problem when switching from
   PLLCK to SLCK.  See errata 39.4.4.2.  */
void
sys_power_mode_low (void)
    __attribute__ ((section(".ramtext")));


void
sys_power_mode_low (void)
{
    /* Deactivating the brownout detector saves 20 uA; this requires
       programming of the GPNVM bits.  */
       
    /* Disabling the UDP saves ??? uA.  Connecting the USB port pins
       to ground also saves about 100 uA.  */
    sys_udp_disable ();

#if 0
    /* TODO.  */

    /* Switch main clock (MCK) from PLLCLK to SLCK.  Note the prescale
       (PRES) and clock source (CSS) fields cannot be changed at the
       same time.  We first switch from the PLLCLK to SLCK then set
       the prescaler to divide by 64. */
    PMC->PMC_MCKR = (PMC->PMC_MCKR & AT91C_PMC_PRES)
        | AT91C_PMC_CSS_SLOW_CLK;

    while (!(PMC->PMC_SR & AT91C_PMC_MCKRDY))
        continue;
    
    /* Set prescaler to divide by 64.  */
    PMC->PMC_MCKR = (PMC->PMC_MCKR & AT91C_PMC_CSS)
        | AT91C_PMC_PRES_CLK_64;

    while (!(PMC->PMC_SR & AT91C_PMC_MCKRDY))
        continue;

    /* Disable PLL.  */
    PMC->PMC_PLLR = 0;

    /* Disable main oscillator.  */
    PMC->PMC_MOR = 0;

    /* Switch voltage regulator to standby (low-power) mode.
       This reduces its static current requirement from 100 uA to 25 uA.  */
    VREG->VREG_MR |= AT91C_VREG_PSTDBY;
#endif
}


void
sys_power_mode_normal (void)
{
#if 0
    /* TODO.  */
    /* Switch voltage regulator to normal mode.  */
    VREG->VREG_MR &= ~AT91C_VREG_PSTDBY;

#endif

    sys_clock_init ();
}


void sys_cpu_idle (void)
{
#if 0
    /* TODO.  */

    /* Turn off CPU clock after current instruction.  It will be
       re-enabled when an interrupt occurs.  */
    PMC->PMC_SCDR = PMC_PCK;

    while ((PMC->PMC_SCSR & PMC_PCK) != PMC_PCK)
        continue;
#endif
}


void
sys_sleep (void)
{
    sys_power_mode_low ();

    /* Disable processor clock and wait for interrupt.  */
    sys_cpu_idle ();

    sys_power_mode_normal ();
}


void
sys_watchdog_reset (void)
{
    WDT->WDT_CR = 0xA5000000 | WDT_CR_WDRSTT;
}


void
sys_watchdog_enable (void)
{
    /* Enable watchdog with 2s timeout.  */
    WDT->WDT_MR = WDT_MR_WDD(0x200) | WDT_MR_WDRSTEN | WDT_MR_WDDBGHLT;
    sys_watchdog_reset ();
}


void
sys_udp_disable (void)
{
#if 0
    /* TODO.  */

    /* The UDP is enabled by default.  To disable the UDP it is
       necessary to turn on the UDP clock, disable the UDP, then turn
       the clock off again.  */
    PMC->PMC_PCER |= (1 << AT91C_ID_UDP);

    UDP->UDP_TXVC |= AT91C_UDP_TXVDIS;

    PMC->PMC_PCDR |= (1 << AT91C_ID_UDP);
#endif
}


void
sys_udp_enable (void)
{
#if 0
    /* TODO.  */
    PMC->PMC_PCER |= (1 << AT91C_ID_UDP);
    UDP->UDP_TXVC &= ~AT91C_UDP_TXVDIS;
#endif
}

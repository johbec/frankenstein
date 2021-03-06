#ifndef _COMMON_H
#define _COMMON_H
void cont();

#define EMULATED


#include <frankenstein.h>
#include <frankenstein/threadx/threading.h>

//utils
int ret1() { return 1; }
int ret0() { return 0; }
void die() {  print_caller(); print(" die();\n"); exit(-1);}
void clean_exit() {  exit(0);}

#include "fwdefs.h"
#include "hci.h"
#include "lm.h"
#include "timer.h"
#include "dynamic_memory.h"


/*
Hook for Peripheral UART
*/
void puart_write_hook(char c) {
    print("\033[;34m");
    write(2, &c, 1);
    print("\033[;00m");
}


/*
Queu
*/

//probably mpaf timer/event
struct queue_entry {
    void *next;
    int maybe_flags;
    void *sometimes_callback;
    void *callback_arg;
    char unknwn[];
} *queue_entry;

void msgqueue_Put_hook(struct saved_regs *regs, void *arg) {
    void *queue = (void *)regs->r0;
    struct queue_entry *item = (void *)regs->r1;
    print_caller();
    print(" msgqueue_Put_hook(");
    print_ptr(queue);
    print(", ");
    print_ptr(item);
    print(");\n");
    print("     ");print_var(item->maybe_flags)
    print("     ");print_var(item->sometimes_callback)
    print("     ");print_var(item->callback_arg)
    print("     "); hexdump(item, 32);
}

/*
This
*/

void print_thrd_bcmbt(uint32_t thrd) {
    switch (thrd) {
        case 0x249e58:
            print("bttransport");
            break;

        case 0x20beb4:
            print("lm");
            break;

        case 0x20a578:
            print("idle");
            break;

        case 0x20a1fc:
            print("mpaf")
            break;

        default:
            print_ptr(_tx_thread_current_ptr);
    }
}


/*
    global code patching
*/

#define idle_loop_ptr (0x024de64)

uint32_t _tx_v7m_get_and_disable_int();
void _tx_v7m_set_int(uint32_t);
uint32_t _tx_v7m_get_int();


void synch_GetXSPRExceptionNum();
void osapi_interruptContext();
void btclk_AdvanceNatClk_clkpclk();


void pmu_sds_ReadSnappedClocks();
void bcs_pmuSleepEnable();
void bcs_kernelBcsShutdownCheck();

void patch_code() {
    //ThreadX basics
    patch_return(_tx_v7m_get_and_disable_int);
    patch_return(_tx_v7m_set_int);
    patch_return(_tx_v7m_get_int);
    patch_jump(_tx_thread_system_return, _tx_thread_system_return_hook);

    patch_return(osapi_interruptContext);
    //patch_jump(osapi_interruptContext, _tx_thread_system_return);
    //patch_return(osapi_interruptContext);

    //Functions, we do not support
    patch_return(0xa43ee);  //synch_GetXSPRExceptionNum
    patch_return(0x20ffba); //get_and_disable_int 2nd ed?!
    patch_return(0x20ffb2); //get_and_disable_int 2nd ed?!
    patch_return(btclk_DelayXus);
    patch_return(btclk_Wait4PclkChange);
    patch_return(btclk_AdvanceNatClk_clkpclkHWWA);
    patch_return(btclk_AdvanceNatClk_clkpclk);

    //Show thread names
    print_thrd = print_thrd_bcmbt;

    //Relplace return from interrupt addr with exit
    idle_loop = clean_exit;

    //Watchdog HW Reset
    patch_jump(wdog_generate_hw_reset, &die);
    //Enable Peripheral UART
    patch_jump(puart_write, &puart_write_hook);
    //Trace dbfw Assert Fatals
    trace(dbfw_assert_fatal, 1, false);

    //Disable NV RAM
    patch_return(wiced_hal_read_nvram);
    trace(wiced_hal_read_nvram, 4, true);
    patch_return(wiced_hal_write_nvram);
    trace(wiced_hal_write_nvram, 4, true);

    //Enable Osapi Timers
    add_timer_hooks();

    hci_install_hooks();

    add_lm_hooks();
    bcs_add_hooks();


    //add heap sanitizer
    init_dynamic_memory_sanitizer();


    patch_return(pmu_sds_ReadSnappedClocks);
    patch_return(bcs_pmuSleepEnable);
}

#endif

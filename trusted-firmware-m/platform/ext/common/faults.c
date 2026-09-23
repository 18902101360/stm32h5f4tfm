/*
 * SPDX-FileCopyrightText: Copyright The TrustedFirmware-M Contributors
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "config_tfm.h"

#include "tfm_hal_device_header.h"
#include "tfm_log.h"
#include "utilities.h"
/* "exception_info.h" must be the last include because of the IAR pragma */
#include "exception_info.h"

static void panic_print_fault(const char *name)
{
    ERROR_RAW("\r\nPANIC %s vect=0x%x cfsr=0x%08x hfsr=0x%08x sfsr=0x%08x bfar=0x%08x mmfar=0x%08x sfar=0x%08x\r\n",
              name,
              (unsigned int)(SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk),
              (unsigned int)SCB->CFSR,
              (unsigned int)SCB->HFSR,
              (unsigned int)SAU->SFSR,
              (unsigned int)SCB->BFAR,
              (unsigned int)SCB->MMFAR,
              (unsigned int)SAU->SFAR);
}

void C_HardFault_Handler(void)
{
    /* A HardFault may indicate corruption of secure state, so it is essential
     * that Non-secure code does not regain control after one is raised.
     * Returning from this exception could allow a pending NS exception to be
     * taken, so the current solution is not to return.
     */
    panic_print_fault("HardFault");
    tfm_core_panic();
}

EXCEPTION_INFO_IAR_REQUIRED
__attribute__((naked)) void HardFault_Handler(void)
{
    EXCEPTION_INFO();

    __ASM volatile(
        "bl        C_HardFault_Handler     \n"
        "b         .                       \n"
    );
}

void C_MemManage_Handler(void)
{
    /* A MemManage fault may indicate corruption of secure state, so it is
     * essential that Non-secure code does not regain control after one is
     * raised. Returning from this exception could allow a pending NS exception
     * to be taken, so the current solution is to panic.
     */
    panic_print_fault("MemManage");
    tfm_core_panic();
}

EXCEPTION_INFO_IAR_REQUIRED
__attribute__((naked)) void MemManage_Handler(void)
{
    EXCEPTION_INFO();

    __ASM volatile(
        "bl        C_MemManage_Handler     \n"
        "b         .                       \n"
    );
}

void C_BusFault_Handler(void)
{
    /* A BusFault may indicate corruption of secure state, so it is essential
     * that Non-secure code does not regain control after one is raised.
     * Returning from this exception could allow a pending NS exception to be
     * taken, so the current solution is to panic.
     */
    panic_print_fault("BusFault");
    tfm_core_panic();
}

EXCEPTION_INFO_IAR_REQUIRED
__attribute__((naked)) void BusFault_Handler(void)
{
    EXCEPTION_INFO();

    __ASM volatile(
        "bl        C_BusFault_Handler      \n"
        "b         .                       \n"
    );
}

void C_SecureFault_Handler(void)
{
    /* A SecureFault may indicate corruption of secure state, so it is essential
     * that Non-secure code does not regain control after one is raised.
     * Returning from this exception could allow a pending NS exception to be
     * taken, so the current solution is to panic.
     */
    panic_print_fault("SecureFault");
    tfm_core_panic();
}

EXCEPTION_INFO_IAR_REQUIRED
__attribute__((naked)) void SecureFault_Handler(void)
{
    EXCEPTION_INFO();

    __ASM volatile(
        "bl        C_SecureFault_Handler   \n"
        "b         .                       \n"
    );
}

void C_UsageFault_Handler(void)
{
    panic_print_fault("UsageFault");
    tfm_core_panic();
}

EXCEPTION_INFO_IAR_REQUIRED
__attribute__((naked)) void UsageFault_Handler(void)
{
    EXCEPTION_INFO();

    __ASM volatile(
        "bl        C_UsageFault_Handler   \n"
        "b         .                      \n"
    );
}

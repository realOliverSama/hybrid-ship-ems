/**
  ******************************************************************************
  * @file    fault_fixture.c
  * @brief   Layer-2 acceptance fault-injection fixture (ISOLATED TEST COPY ONLY).
  ******************************************************************************
  */
#include "fault_fixture.h"

/* LED1 half-period configuration value.
 * FAULT_LED_PERIOD build: 50 ms (=> 100 ms full period), deliberately
 * deviating from the 400 ms baseline contract.
 * All other builds: 200 ms (=> 400 ms full period), matching the contract. */
#if defined(FAULT_LED_PERIOD)
volatile int32_t led1_toggle_half_period_ms = 50;
#else
volatile int32_t led1_toggle_half_period_ms = 200;
#endif

volatile uint32_t startup_stall_marker = 0u;

/**
 * @brief  Deterministic HardFault trigger (D-30).
 *         UDF #0 (0xDE00) is a permanently undefined Thumb instruction:
 *         it raises an UNDEFINSTR UsageFault which escalates to HardFault
 *         because the UsageFault handler is not enabled by default.
 * @note   noinline so the fault PC resolves to this named symbol.
 */
__attribute__((noinline)) void fault_trigger_hardfault(void)
{
#if defined(FAULT_HARDFAULT)
    __asm volatile (".inst 0xDE00");
#endif
}

/**
 * @brief  Deterministic startup stall (D-50).
 *         Sets a debugger-readable marker, then spins forever.
 *         Does not touch the debug port, clocks or security config.
 * @note   noinline so a halt lands inside this named symbol.
 */
__attribute__((noinline)) void fault_startup_stall(void)
{
    startup_stall_marker = STARTUP_STALL_MAGIC;
    for (;;)
    {
        __asm volatile ("nop");
    }
}

void fault_fixture_pre_loop_hook(void)
{
#if defined(FAULT_STARTUP_STALL)
    fault_startup_stall();
#endif
}

void fault_fixture_loop_hook(void)
{
#if defined(FAULT_HARDFAULT)
    fault_trigger_hardfault();
#endif
}

/**
  ******************************************************************************
  * @file    fault_fixture.h
  * @brief   Layer-2 acceptance fault-injection fixture (ISOLATED TEST COPY ONLY).
  *          Select at most one variant via -DFAULT_VARIANT=<name> at CMake
  *          configure time:
  *            FAULT_HARDFAULT      - deterministic undefined instruction
  *            FAULT_LED_PERIOD     - LED1 period deviates from 400 ms contract
  *            FAULT_STARTUP_STALL  - named stall before the main loop + marker
  *          With no variant defined the fixture is behavior-neutral.
  ******************************************************************************
  */
#ifndef FAULT_FIXTURE_H
#define FAULT_FIXTURE_H

#include <stdint.h>

/* LED1 toggle half-period in milliseconds.
 * Baseline contract: 200 ms half-period => 400 ms full blink period.
 * Signed and ELF-locatable so D-40 can resolve it from the symbol table and
 * compare the runtime value against the contract without guessing. */
extern volatile int32_t led1_toggle_half_period_ms;

/* D-50 startup-stall marker, readable through the debugger while stalled. */
extern volatile uint32_t startup_stall_marker;
#define STARTUP_STALL_MAGIC 0x57A11ED1u

/* Named, noinline trigger entry points (symbol-locatable in every build). */
void fault_trigger_hardfault(void);
void fault_startup_stall(void);

/* Hook points called from main.c; no-ops unless a fault variant is enabled. */
void fault_fixture_pre_loop_hook(void);
void fault_fixture_loop_hook(void);

#endif /* FAULT_FIXTURE_H */

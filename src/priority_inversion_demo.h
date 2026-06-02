#ifndef PRIORITY_INVERSION_DEMO_H
#define PRIORITY_INVERSION_DEMO_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Run the full priority inversion demonstration:
 * Phase 1: demonstrates the BUG using binary semaphore
 * Phase 2: demonstrates the FIX using mutex
 * Outputs timestamped messages over UART
 * Blocks the caller until the demonstration is complete (~10 seconds total)
 */
void priority_inversion_demo_run(void);

#endif /* PRIORITY_INVERSION_DEMO_H */
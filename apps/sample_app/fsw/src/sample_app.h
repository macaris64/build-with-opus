#ifndef SAMPLE_APP_H
#define SAMPLE_APP_H

/*
 * sample_app.h — Public interface for the SAMPLE cFS Application.
 *
 * This app demonstrates the canonical cFS application structure:
 *   - Software Bus (CFE_SB) for inter-app communication
 *   - Event Services (CFE_EVS) for all runtime logging
 *   - Executive Services (CFE_ES) for lifecycle management
 *
 * MISRA C:2012 compliance: all deviations documented inline.
 */

#include "cfe.h"
#include "sample_app_events.h"
#include "sample_app_version.h"
#include "mids.h"

/* Command codes */
#define SAMPLE_APP_NOOP_CC      0U
#define SAMPLE_APP_RESET_CC     1U

/* Maximum depth of the command pipe */
#define SAMPLE_APP_PIPE_DEPTH   10U

/* Application state — all state in one static struct; no heap allocation */
typedef struct
{
    uint32           RunStatus;
    CFE_SB_PipeId_t  CmdPipe;
    uint16           CmdCounter;
    uint16           ErrCounter;
} SAMPLE_APP_Data_t;

/* Application entry point — registered with CFE_ES */
void SAMPLE_APP_AppMain(void);

#endif /* SAMPLE_APP_H */

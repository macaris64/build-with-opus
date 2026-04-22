#ifndef SAMPLE_APP_EVENTS_H
#define SAMPLE_APP_EVENTS_H

/* Event IDs for SAMPLE_APP. Each ID must be unique within this app.
 * The CFE Event Services (CFE_EVS) module uses these to filter and route
 * event messages to ground or onboard consumers. */
typedef enum
{
    SAMPLE_APP_STARTUP_INF_EID   = 1, /* App initialized successfully */
    SAMPLE_APP_CMD_ERR_EID       = 2, /* Unrecognized or malformed command received */
    SAMPLE_APP_CMD_NOOP_INF_EID  = 3, /* NOOP command received (version confirmation) */
} SAMPLE_APP_EventIds_t;

#define SAMPLE_APP_EVENT_COUNT 3U

#endif /* SAMPLE_APP_EVENTS_H */

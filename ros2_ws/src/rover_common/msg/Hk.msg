# Generic housekeeping telemetry frame forwarded by tm_bridge.
# Fixed 32-byte payload keeps the CCSDS packet size predictable.
uint32 seq
float64 timestamp
uint8[32] payload

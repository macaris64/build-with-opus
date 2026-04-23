// This module's BE-only parser is the functional seed for
// `ccsds_wire::primary_header`; the Phase C Step 2 scaffolding PR migrates
// the type, error enum, and tests out per
// `docs/architecture/06-ground-segment-rust.md §1.6`. The allowances below
// keep the existing code building against the Phase 01 workspace lint
// block without pre-emptively refactoring what §1.6 will replace
// wholesale. Remove them at migration time.
#![allow(clippy::indexing_slicing, clippy::missing_panics_doc)]

/// CCSDS Space Packet Protocol secondary header (telemetry).
/// Layout matches the cFS telemetry header defined in `cfe_msg.h`.
#[derive(Debug, PartialEq, Eq)]
pub struct TelemetryPacket {
    /// Application Process Identifier (11 bits, masked from primary header)
    pub apid: u16,
    /// Packet sequence count (14 bits)
    pub sequence_count: u16,
    /// Packet data length in bytes (value = `total_length` - 7)
    pub data_length: u16,
}

/// Errors that can occur when parsing a raw CCSDS telemetry packet.
#[derive(Debug, PartialEq, Eq)]
pub enum ParseError {
    /// Buffer is shorter than the 6-byte primary header
    TooShort,
    /// Version field is not 0b000 (CCSDS version 1)
    InvalidVersion,
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ParseError::TooShort => write!(f, "packet buffer too short (minimum 6 bytes)"),
            ParseError::InvalidVersion => write!(f, "invalid CCSDS version field (expected 0)"),
        }
    }
}

/// Parse a raw byte slice as a CCSDS primary header.
///
/// # Errors
/// Returns [`ParseError::TooShort`] if `raw` has fewer than 6 bytes.
/// Returns [`ParseError::InvalidVersion`] if the version field is not 0.
pub fn parse(raw: &[u8]) -> Result<TelemetryPacket, ParseError> {
    const CCSDS_PRIMARY_HEADER_LEN: usize = 6;

    if raw.len() < CCSDS_PRIMARY_HEADER_LEN {
        return Err(ParseError::TooShort);
    }

    /* CCSDS primary header byte 0, bits [7:5] = version (must be 0b000) */
    let version = (raw[0] >> 5) & 0x07;
    if version != 0 {
        return Err(ParseError::InvalidVersion);
    }

    /* APID occupies bits [10:0] of the first two bytes */
    let apid = u16::from_be_bytes([raw[0] & 0x07, raw[1]]);

    /* Sequence count occupies bits [13:0] of bytes 2-3 */
    let sequence_count = u16::from_be_bytes([raw[2] & 0x3F, raw[3]]);

    /* Data length field is bytes 4-5 */
    let data_length = u16::from_be_bytes([raw[4], raw[5]]);

    Ok(TelemetryPacket {
        apid,
        sequence_count,
        data_length,
    })
}

#[cfg(test)]
#[allow(clippy::expect_used)]
mod tests {
    use super::*;

    /// Minimal valid CCSDS primary header: version=0, APID=0x123, seq=1, len=4
    fn valid_header() -> [u8; 6] {
        [
            0x01, 0x23, /* version=0, APID=0x123 */
            0xC0, 0x01, /* seq flags=0b11, seq count=1 */
            0x00, 0x04, /* data length = 4 */
        ]
    }

    #[test]
    fn test_parse_valid_packet_returns_correct_fields() {
        let raw = valid_header();
        let pkt = parse(&raw).expect("valid header must parse successfully");

        assert_eq!(pkt.apid, 0x0123);
        assert_eq!(pkt.sequence_count, 1);
        assert_eq!(pkt.data_length, 4);
    }

    #[test]
    fn test_parse_too_short_returns_error() {
        let raw = [0x00u8; 5]; /* one byte short */
        assert_eq!(parse(&raw), Err(ParseError::TooShort));
    }

    #[test]
    fn test_parse_empty_slice_returns_too_short() {
        assert_eq!(parse(&[]), Err(ParseError::TooShort));
    }

    #[test]
    fn test_parse_invalid_version_returns_error() {
        let mut raw = valid_header();
        raw[0] |= 0x20; /* set version bit 5 → version = 0b001 */
        assert_eq!(parse(&raw), Err(ParseError::InvalidVersion));
    }

    #[test]
    fn test_parse_extra_bytes_beyond_header_are_ignored() {
        let mut raw = valid_header().to_vec();
        raw.extend_from_slice(&[0xFF, 0xFF, 0xFF]); /* payload bytes */
        let pkt = parse(&raw).expect("extra bytes should not cause failure");
        assert_eq!(pkt.apid, 0x0123);
    }
}

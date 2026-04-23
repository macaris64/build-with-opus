//! Fluent builder for CCSDS Space Packets (ground TC/TM path).
//!
//! Phase 09 locus. Produces a validated `Vec<u8>` encoding a complete
//! Space Packet (primary + secondary + user data). Allocation is
//! intentional — this is the ground-side path, not hot-path FSW.
//! [`SpacePacket::parse`][crate::SpacePacket::parse] round-trips the output.
//!
//! # CCSDS `data_length` computation
//!
//! Per CCSDS 133.0-B-2: `data_length = total_bytes − 7`. For a Phase 09
//! packet (`6 B primary + 10 B secondary + N B user data`):
//!
//! ```text
//! data_length = 10 + user_data.len() − 1 = 9 + user_data.len()
//! ```
//!
//! The minimum is `9` (empty payload → 16-byte total packet).
//!
//! # Definition sites
//! - `docs/architecture/06-ground-segment-rust.md §2.7`
//! - `SYS-REQ-0020`; `Q-C8`.

use crate::{
    Apid, CcsdsError, Cuc, FuncCode, InstanceId, PacketDataLength, PacketType, PrimaryHeader,
    SecondaryHeader, SequenceCount,
};

/// Fluent builder for a complete CCSDS Space Packet.
///
/// Constructed via [`PacketBuilder::tm`] or [`PacketBuilder::tc`]; method
/// chaining sets optional fields; [`PacketBuilder::build`] encodes and
/// validates.
///
/// Fields with CCSDS zero-defaults are pre-initialised:
/// `sequence_count = 0`, `cuc = Cuc { coarse: 0, fine: 0 }`.
/// `func_code` and `instance_id` have no valid zero-default — both
/// reject `0` — so `build` returns an error if they are not set.
pub struct PacketBuilder {
    apid: Apid,
    packet_type: PacketType,
    sequence_count: Option<SequenceCount>,
    cuc: Cuc,
    func_code: Option<FuncCode>,
    instance_id: Option<InstanceId>,
    user_data: Vec<u8>,
}

impl PacketBuilder {
    /// Start a TM (telemetry) packet builder.
    #[must_use]
    pub fn tm(apid: Apid) -> Self {
        Self::new(apid, PacketType::Tm)
    }

    /// Start a TC (telecommand) packet builder.
    #[must_use]
    pub fn tc(apid: Apid) -> Self {
        Self::new(apid, PacketType::Tc)
    }

    /// Set the function code.
    #[must_use]
    pub fn func_code(mut self, v: FuncCode) -> Self {
        self.func_code = Some(v);
        self
    }

    /// Set the instance ID.
    #[must_use]
    pub fn instance_id(mut self, v: InstanceId) -> Self {
        self.instance_id = Some(v);
        self
    }

    /// Set the CUC time stamp.
    #[must_use]
    pub fn cuc(mut self, v: Cuc) -> Self {
        self.cuc = v;
        self
    }

    /// Set the sequence count (default: `0`).
    #[must_use]
    pub fn sequence_count(mut self, v: SequenceCount) -> Self {
        self.sequence_count = Some(v);
        self
    }

    /// Set the user-data payload (cloned into the builder; default: empty).
    #[must_use]
    pub fn user_data(mut self, data: &[u8]) -> Self {
        self.user_data = data.to_vec();
        self
    }

    /// Encode the packet into a new `Vec<u8>`.
    ///
    /// # Errors
    ///
    /// - [`CcsdsError::FuncCodeReserved`] if [`PacketBuilder::func_code`]
    ///   was never called.
    /// - [`CcsdsError::InstanceIdReserved`] if [`PacketBuilder::instance_id`]
    ///   was never called.
    /// - [`CcsdsError::LengthMismatch`] if the computed `data_length` would
    ///   exceed `u16::MAX` (user data > 65 526 bytes).
    pub fn build(self) -> Result<Vec<u8>, CcsdsError> {
        let func_code = self.func_code.ok_or(CcsdsError::FuncCodeReserved)?;
        let instance_id = self.instance_id.ok_or(CcsdsError::InstanceIdReserved)?;

        // Sequence count defaults to 0 (valid: 0..=0x3FFF).
        // SequenceCount::new(0) only fails if 0 > 0x3FFF — mathematically
        // impossible; the `?` is kept for the single auditable error path.
        let sequence_count = self
            .sequence_count
            .map_or_else(|| SequenceCount::new(0), Ok)?;

        // data_length = SecondaryHeader::LEN + user_data.len() - 1
        //             = 9 + user_data.len()
        let raw_dl = SecondaryHeader::LEN
            .checked_add(self.user_data.len())
            .and_then(|n| n.checked_sub(1))
            .ok_or(CcsdsError::LengthMismatch {
                declared: usize::MAX,
                actual: usize::from(u16::MAX) + 7,
            })?;

        let dl = u16::try_from(raw_dl).map_err(|_| CcsdsError::LengthMismatch {
            declared: raw_dl,
            actual: usize::from(u16::MAX) + 7,
        })?;

        let data_length = PacketDataLength::new(dl);
        let secondary = SecondaryHeader::new(self.cuc, func_code, instance_id);
        let primary = PrimaryHeader::new(self.apid, self.packet_type, sequence_count, data_length);

        let mut pb = [0u8; PrimaryHeader::LEN];
        let mut sb = [0u8; SecondaryHeader::LEN];
        primary.encode(&mut pb);
        secondary.encode(&mut sb);

        let total = PrimaryHeader::LEN + SecondaryHeader::LEN + self.user_data.len();
        let mut out = Vec::with_capacity(total);
        out.extend_from_slice(&pb);
        out.extend_from_slice(&sb);
        out.extend_from_slice(&self.user_data);
        Ok(out)
    }

    fn new(apid: Apid, packet_type: PacketType) -> Self {
        Self {
            apid,
            packet_type,
            sequence_count: None,
            cuc: Cuc { coarse: 0, fine: 0 },
            func_code: None,
            instance_id: None,
            user_data: Vec::new(),
        }
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
#[allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::cast_possible_truncation
)]
mod tests {
    use super::*;
    use crate::SpacePacket;

    // Helpers
    fn apid() -> Apid {
        Apid::new(0x100).unwrap()
    }
    fn fc() -> FuncCode {
        FuncCode::new(0x0042).unwrap()
    }
    fn id() -> InstanceId {
        InstanceId::new(3).unwrap()
    }

    // --- TM roundtrip -------------------------------------------------------
    // Given: a fully specified TM builder with 4-byte payload.
    // When:  build(), then SpacePacket::parse().
    // Then:  all fields are preserved; total_len() matches byte count.
    #[test]
    fn test_builder_tm_roundtrip() {
        let cuc = Cuc {
            coarse: 1_000_000,
            fine: 500,
        };
        let payload = [0xDE_u8, 0xAD, 0xBE, 0xEF];
        let bytes = PacketBuilder::tm(apid())
            .func_code(fc())
            .instance_id(id())
            .cuc(cuc)
            .user_data(&payload)
            .build()
            .unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.apid(), apid());
        assert_eq!(pkt.primary.packet_type(), PacketType::Tm);
        assert_eq!(pkt.secondary.func_code(), fc());
        assert_eq!(pkt.secondary.instance_id(), id());
        assert_eq!(pkt.secondary.time(), cuc);
        assert_eq!(pkt.user_data, &payload);
        assert_eq!(pkt.total_len(), bytes.len());
    }

    // --- TC type bit --------------------------------------------------------
    // Given: tc() builder (no user data).
    // When:  build() + parse().
    // Then:  packet_type() == Tc.
    #[test]
    fn test_builder_tc_type_bit() {
        let bytes = PacketBuilder::tc(Apid::new(0x184).unwrap())
            .func_code(FuncCode::new(0x8100).unwrap())
            .instance_id(InstanceId::new(1).unwrap())
            .build()
            .unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.packet_type(), PacketType::Tc);
        assert_eq!(pkt.primary.apid(), Apid::new(0x184).unwrap());
    }

    // --- Sequence count preserved --------------------------------------------
    // Given: .sequence_count(sc) explicitly set.
    // When:  build() + parse().
    // Then:  sequence_count() returns the same value.
    #[test]
    fn test_builder_sequence_count_preserved() {
        let sc = SequenceCount::new(0x1234).unwrap();
        let bytes = PacketBuilder::tm(apid())
            .func_code(fc())
            .instance_id(id())
            .sequence_count(sc)
            .build()
            .unwrap();
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.primary.sequence_count(), sc);
    }

    // --- Default empty user data --------------------------------------------
    // Given: builder with no .user_data() call.
    // When:  build().
    // Then:  bytes.len() == 16; parse() gives empty user_data.
    #[test]
    fn test_builder_default_empty_payload() {
        let bytes = PacketBuilder::tm(apid())
            .func_code(fc())
            .instance_id(id())
            .build()
            .unwrap();
        assert_eq!(bytes.len(), 16);
        let pkt = SpacePacket::parse(&bytes).unwrap();
        assert_eq!(pkt.user_data, &[] as &[u8]);
    }

    // --- Missing func_code → FuncCodeReserved --------------------------------
    // Given: no .func_code() call.
    // When:  build().
    // Then:  Err(FuncCodeReserved).
    #[test]
    fn test_builder_missing_func_code() {
        let result = PacketBuilder::tm(apid()).instance_id(id()).build();
        assert_eq!(result, Err(CcsdsError::FuncCodeReserved));
    }

    // --- Missing instance_id → InstanceIdReserved ---------------------------
    // Given: no .instance_id() call.
    // When:  build().
    // Then:  Err(InstanceIdReserved).
    #[test]
    fn test_builder_missing_instance_id() {
        let result = PacketBuilder::tm(apid()).func_code(fc()).build();
        assert_eq!(result, Err(CcsdsError::InstanceIdReserved));
    }

    // --- data_length overflow → LengthMismatch ------------------------------
    // Given: user_data.len() = 65527 (9 + 65527 = 65536 > u16::MAX).
    // When:  build().
    // Then:  Err(LengthMismatch { .. }).
    #[test]
    fn test_builder_data_length_overflow() {
        let huge = vec![0u8; usize::from(u16::MAX) - 8]; // len = 65527
        let result = PacketBuilder::tm(apid())
            .func_code(fc())
            .instance_id(id())
            .user_data(&huge)
            .build();
        assert!(
            matches!(result, Err(CcsdsError::LengthMismatch { .. })),
            "expected LengthMismatch, got {result:?}",
        );
    }
}

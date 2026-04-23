//! Property-based tests for `ccsds_wire`. Populated by Phases 03–09 per
//! `IMPLEMENTATION_GUIDE.md`. Phase 02 shipped this file empty so the
//! integration-test harness wiring was in place before any test body landed.
//! Phase 06 lands the first body: a single `prop_cuc_roundtrip` scaffold
//! covering the CUC codec's encode/decode invariant. Phase 10 milestone
//! expands the battery (e.g. `prop_rejects_invalid_pfield`,
//! `prop_rejects_short_buffer`) once the primary-header codec is online.

// Test-only conveniences: workspace denies these for production code.
// indexing_slicing: packet bytes are known to have length >= 16 at call site.
#![allow(
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used,
    clippy::indexing_slicing
)]

use ccsds_wire::{
    Apid, CcsdsError, Cuc, FuncCode, InstanceId, PacketBuilder, PacketDataLength, PacketType,
    PrimaryHeader, SecondaryHeader, SequenceCount, SpacePacket, P_FIELD,
};
use proptest::prelude::*;

proptest! {
    /// For any `(coarse, fine)` pair in the full `u32 × u16` domain:
    /// `Cuc::decode_be(encode_be(x)) == x` and the encoded P-Field byte is
    /// `0x2F`. Covers Q-C8 (BE wire) and Q-C6 (P-Field pinning) together.
    #[test]
    fn prop_cuc_roundtrip(coarse in any::<u32>(), fine in any::<u16>()) {
        let original = Cuc { coarse, fine };
        let mut buf = [0u8; 7];
        original.encode_be(&mut buf);
        prop_assert_eq!(buf[0], P_FIELD);
        let decoded = Cuc::decode_be(&buf).unwrap();
        prop_assert_eq!(decoded, original);
    }

    /// For any valid `(apid, packet_type, seq_count, data_length)`:
    /// `PrimaryHeader::decode(encode(h)) == h`. Covers Q-C8 BE layout for
    /// the 6-byte identification + sequence control fields.
    #[test]
    fn prop_primary_header_roundtrip(
        apid in 0u16..=0x7FFu16,
        pt in 0u8..=1u8,
        seq in 0u16..=0x3FFFu16,
        data_len in any::<u16>(),
    ) {
        let apid_v = Apid::new(apid).unwrap();
        let pt_v = PacketType::from_bit(pt);
        let seq_v = SequenceCount::new(seq).unwrap();
        let pdl_v = PacketDataLength::new(data_len);
        let original = PrimaryHeader::new(apid_v, pt_v, seq_v, pdl_v);
        let mut buf = [0u8; PrimaryHeader::LEN];
        original.encode(&mut buf);
        let decoded = PrimaryHeader::decode(&buf).unwrap();
        prop_assert_eq!(decoded, original);
    }

    /// For any valid `(coarse, fine, func_code, instance_id)`:
    /// `SecondaryHeader::decode(encode(h)) == h`. Covers Q-C8 BE layout for
    /// the CUC time stamp, func_code, and instance_id bytes.
    #[test]
    fn prop_secondary_header_roundtrip(
        coarse in any::<u32>(),
        fine in any::<u16>(),
        func_code in 1u16..=u16::MAX,
        instance_id in 1u8..=u8::MAX,
    ) {
        let fc_v = FuncCode::new(func_code).unwrap();
        let iid_v = InstanceId::new(instance_id).unwrap();
        let original = SecondaryHeader::new(Cuc { coarse, fine }, fc_v, iid_v);
        let mut buf = [0u8; SecondaryHeader::LEN];
        original.encode(&mut buf);
        let decoded = SecondaryHeader::decode(&buf).unwrap();
        prop_assert_eq!(decoded, original);
    }

    /// For any complete valid packet built by `PacketBuilder`:
    /// `SpacePacket::parse(build(components))` reproduces every component.
    /// Covers the full encode → wire → decode round-trip at the packet level.
    #[test]
    fn prop_space_packet_roundtrip(
        apid in 0u16..=0x7FFu16,
        pt in 0u8..=1u8,
        seq in 0u16..=0x3FFFu16,
        func_code in 1u16..=u16::MAX,
        instance_id in 1u8..=u8::MAX,
        coarse in any::<u32>(),
        fine in any::<u16>(),
        user_data in proptest::collection::vec(any::<u8>(), 0..=256usize),
    ) {
        let apid_v = Apid::new(apid).unwrap();
        let seq_v = SequenceCount::new(seq).unwrap();
        let fc_v = FuncCode::new(func_code).unwrap();
        let iid_v = InstanceId::new(instance_id).unwrap();
        let cuc_v = Cuc { coarse, fine };

        let builder = if pt == 0 { PacketBuilder::tm(apid_v) } else { PacketBuilder::tc(apid_v) };
        let bytes = builder
            .sequence_count(seq_v)
            .func_code(fc_v)
            .instance_id(iid_v)
            .cuc(cuc_v)
            .user_data(&user_data)
            .build()
            .unwrap();

        let pkt = SpacePacket::parse(&bytes).unwrap();
        prop_assert_eq!(pkt.primary.apid(), apid_v);
        prop_assert_eq!(pkt.primary.packet_type(), PacketType::from_bit(pt));
        prop_assert_eq!(pkt.primary.sequence_count(), seq_v);
        prop_assert_eq!(pkt.secondary.func_code(), fc_v);
        prop_assert_eq!(pkt.secondary.instance_id(), iid_v);
        prop_assert_eq!(pkt.secondary.time(), cuc_v);
        prop_assert_eq!(pkt.user_data, user_data.as_slice());
    }

    /// For any buffer shorter than `HEADER_LEN` (16 B):
    /// `SpacePacket::parse` returns `BufferTooShort { need: 16, got: len }`.
    #[test]
    fn prop_rejects_short_buffer(truncated_len in 0usize..SpacePacket::HEADER_LEN) {
        let buf = vec![0u8; truncated_len];
        let result = SpacePacket::parse(&buf);
        prop_assert_eq!(
            result,
            Err(CcsdsError::BufferTooShort { need: SpacePacket::HEADER_LEN, got: truncated_len })
        );
    }

    /// For any valid primary-header buffer with version bits `[7:5]` forced
    /// to a non-zero value: `PrimaryHeader::decode` returns `InvalidVersion`.
    #[test]
    fn prop_rejects_invalid_version(
        apid in 0u16..=0x7FFu16,
        pt in 0u8..=1u8,
        seq in 0u16..=0x3FFFu16,
        data_len in any::<u16>(),
        version_bits in 1u8..=7u8,
    ) {
        let original = PrimaryHeader::new(
            Apid::new(apid).unwrap(),
            PacketType::from_bit(pt),
            SequenceCount::new(seq).unwrap(),
            PacketDataLength::new(data_len),
        );
        let mut buf = [0u8; PrimaryHeader::LEN];
        original.encode(&mut buf);
        // Overwrite version bits [7:5] with a non-zero value.
        buf[0] = (buf[0] & 0x1F) | (version_bits << 5);
        let result = PrimaryHeader::decode(&buf);
        prop_assert!(matches!(result, Err(CcsdsError::InvalidVersion(_))));
    }

    /// For any valid complete packet with byte 6 (P-Field) replaced by a
    /// value ≠ `0x2F`: `SpacePacket::parse` returns `InvalidPField`.
    #[test]
    fn prop_rejects_invalid_pfield(
        apid in 0u16..=0x7FFu16,
        func_code in 1u16..=u16::MAX,
        instance_id in 1u8..=u8::MAX,
        bad_pfield in (0u8..=0xFFu8).prop_filter("not 0x2F", |b| *b != 0x2F),
    ) {
        let apid_v = Apid::new(apid).unwrap();
        let fc_v = FuncCode::new(func_code).unwrap();
        let iid_v = InstanceId::new(instance_id).unwrap();
        let mut bytes = PacketBuilder::tm(apid_v)
            .func_code(fc_v)
            .instance_id(iid_v)
            .build()
            .unwrap();
        // Corrupt the CUC P-Field at byte 6 (first byte of secondary header).
        bytes[6] = bad_pfield;
        let result = SpacePacket::parse(&bytes);
        prop_assert!(matches!(result, Err(CcsdsError::InvalidPField(_))));
    }
}

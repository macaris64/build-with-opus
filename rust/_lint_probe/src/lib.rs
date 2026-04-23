//! Phase 01 Red-test fixture. Intentionally lint-hostile.
//!
//! Once the Phase 01 workspace lint block is active (Green commit), this
//! crate MUST fail to build with diagnostics for each of:
//!
//! * `unused_imports`     — via `.cargo/config.toml` `rustflags = -D warnings`
//! * `unsafe_code`        — via `[workspace.lints.rust] unsafe_code = deny`
//! * `clippy::panic`      — via `[workspace.lints.clippy] panic = deny`
//! * `clippy::unwrap_used` — via `[workspace.lints.clippy] unwrap_used = deny`
//!
//! `scripts/verify_phase_01.sh` asserts all four appear in the probe's
//! stderr. Do not consume this crate from anywhere else in the tree.

use std::io::Read;

pub fn probe() {
    unsafe {}
    let _: u8 = None::<u8>.unwrap();
    panic!("probe");
}

//! Phase 01 Red-test fixture. Intentionally lint-hostile.
//!
//! Once the Phase 01 workspace lint block is active (Green commit), this
//! crate MUST fail to build with both `unsafe_code` and `unused_imports`
//! diagnostics. Do not consume this crate from anywhere else in the tree.

use std::io::Read;

pub fn probe() {
    unsafe {}
}

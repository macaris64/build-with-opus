#include "rover_examples/tm_bridge_example.hpp"

#include "rover_common/tm_bridge.hpp"

namespace rover_examples
{

std::vector<uint8_t> pack_single_hk(
    uint16_t apid,
    const std::vector<uint8_t> & payload,
    uint32_t tai_coarse,
    uint32_t tai_fine)
{
    /* TmBridge enforces the 65 526 B limit internally and returns empty; mirror
     * that contract with an explicit guard so the branch is independently testable. */
    if (payload.size() > 65526U) {
        return {};
    }
    rover_common::TmBridge bridge(apid);
    return bridge.pack_hk(payload, tai_coarse, tai_fine);
}

std::vector<std::vector<uint8_t>> pack_hk_sequence(
    uint16_t apid,
    const std::vector<std::vector<uint8_t>> & payloads,
    uint32_t tai_coarse,
    uint32_t tai_fine)
{
    rover_common::TmBridge bridge(apid);
    std::vector<std::vector<uint8_t>> out;
    out.reserve(payloads.size());
    for (const auto & p : payloads) {
        auto pkt = bridge.pack_hk(p, tai_coarse, tai_fine);
        if (!pkt.empty()) {
            out.push_back(std::move(pkt));
        }
    }
    return out;
}

}  // namespace rover_examples

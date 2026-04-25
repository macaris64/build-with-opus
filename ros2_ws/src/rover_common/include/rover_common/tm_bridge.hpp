#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <netinet/in.h>

namespace rover_common
{

/* TmBridge — packs ROS 2 housekeeping data into CCSDS TM Space Packets.
 *
 * Encodes the CCSDS primary header (6 B) and secondary header (10 B) per
 * Q-C6 (CUC 7 B time + 2 B func_code + 1 B instance_id) in big-endian byte
 * order per Q-C8.  The 14-bit sequence counter wraps at 0x3FFF per CCSDS 133.
 *
 * Intended use: one TmBridge instance per APID; call pack_hk() on every HK
 * sample; forward the returned bytes to the Prx-1 outbound path.
 */
class TmBridge
{
public:
    /* apid — 11-bit APID (bits above 10 are masked and ignored). */
    explicit TmBridge(uint16_t apid) noexcept;

    /* Pack ROS 2 housekeeping data into a CCSDS TM packet.
     *
     * Returns the fully encoded packet (primary header + secondary header +
     * user data) as a big-endian byte vector, or an empty vector if the
     * payload exceeds the 16-bit data_length field capacity (> 65 526 bytes).
     *
     * tai_coarse: 4-byte TAI coarse time (seconds)
     * tai_fine:   3-byte TAI fine time (sub-seconds, CUC fine field)
     *
     * The internal sequence counter advances by one on each successful call
     * and wraps from 0x3FFF to 0x0000 (14-bit CCSDS counter). */
    std::vector<uint8_t> pack_hk(
        const std::vector<uint8_t> & payload,
        uint32_t tai_coarse,
        uint32_t tai_fine) noexcept;

    /* Reset the sequence counter.  The next pack_hk call will emit a packet
     * with seq == to.  Values above 0x3FFF are masked to 14 bits.
     * Primarily intended for testing. */
    void reset_seq(uint16_t to = 0U) noexcept;

private:
    uint16_t apid_;       /* 11-bit APID, stored pre-masked */
    uint16_t seq_count_;  /* 14-bit CCSDS sequence counter  */
};

/* UdpGateway — non-blocking UDP sender for rover HK SPP forwarding.
 *
 * Constructed with a destination host/port; wraps a SOCK_DGRAM|SOCK_NONBLOCK
 * socket connected at construction time.  send() is non-blocking: EAGAIN is
 * silenced so a busy ground loop never blocks a timer callback.
 */
class UdpGateway
{
public:
    UdpGateway(const std::string & host, uint16_t port) noexcept;
    ~UdpGateway() noexcept;

    /* Non-copyable, non-movable (owns the socket fd). */
    UdpGateway(const UdpGateway &)            = delete;
    UdpGateway & operator=(const UdpGateway &) = delete;

    /* Returns false only on hard errors (socket invalid, or send fails beyond EAGAIN). */
    bool send(const std::vector<uint8_t> & data) noexcept;

private:
    int sock_fd_{-1};
};

}  // namespace rover_common

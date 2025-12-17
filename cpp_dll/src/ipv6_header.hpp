#ifndef IPV6_HEADER_HPP
#define IPV6_HEADER_HPP

#include <algorithm>
#include <asio/ip/address_v6.hpp>
#include <iostream>

// Packet header for IPv6.
//
// The wire format of an IPv6 header is:
//
// 0       4       8       12      16             24             31
// +-------+-------+-------+-------+--------------+---------------+   ---
// |       |    traffic    |                                      |    ^
// |version|     class     |             flow label               |    |
// |  (6)  |               |                                      |    |
// +-------+---------------+-------+--------------+---------------+    |
// |                               |              |               |    |
// |         payload length        |  next header |   hop limit   |    |
// |                               |              |               |    |
// +-------------------------------+--------------+---------------+    |
// |                                                              |    |
// |                                                              |    |
// |                   source address (128)                       |    |
// |                                                              | 36 bytes
// |                                                              |    |
// +--------------------------------------------------------------+    |
// |                                                              |    |
// |                                                              |    |
// |                 destination address (128)                    |    |
// |                                                              |    |
// |                                                              |    v
// +--------------------------------------------------------------+   ---
//
// Note: IPv6 header has a fixed length of 40 bytes, with no optional
// header fields in the base header. Extension headers follow the
// base header if present.

namespace net {
class ipv6_header {
public:
  ipv6_header() { std::fill(rep_, rep_ + sizeof(rep_), 0); }
  ~ipv6_header() noexcept = default;

  ipv6_header(const ipv6_header &ih) noexcept {
    std::copy(ih.rep_, ih.rep_ + sizeof(rep_), rep_);
  }

  ipv6_header(ipv6_header &&ih) noexcept {
    std::copy(ih.rep_, ih.rep_ + sizeof(rep_), rep_);
  }

  unsigned char version() const noexcept { return (rep_[0] >> 4) & 0xF; }
  unsigned char traffic_class() const noexcept {
    return ((rep_[0] & 0xF) << 4) | (rep_[1] >> 4) & 0xF;
  }
  unsigned int flow_label() const noexcept {
    return (rep_[1] & 0xF) << 16 | (rep_[2] << 8) | rep_[3];
  }
  unsigned short payload_length() const noexcept {
    return (rep_[4] << 8) + rep_[5];
  }
  unsigned char next_header() const noexcept { return rep_[6]; }
  unsigned int hop_limit() const noexcept { return rep_[7]; }

  asio::ip::address_v6 source_address() const {
    asio::ip::address_v6::bytes_type bytes = {
        {rep_[8], rep_[9], rep_[10], rep_[11], rep_[12], rep_[13], rep_[14],
         rep_[15], rep_[16], rep_[17], rep_[18], rep_[19], rep_[20], rep_[21],
         rep_[22], rep_[23]}};
    return asio::ip::address_v6(bytes);
  }

  asio::ip::address_v6 destination_address() const {
    asio::ip::address_v6::bytes_type bytes = {
        {rep_[24], rep_[25], rep_[26], rep_[27], rep_[28], rep_[29], rep_[30],
         rep_[31], rep_[32], rep_[33], rep_[34], rep_[35], rep_[36], rep_[37],
         rep_[38], rep_[39]}};
    return asio::ip::address_v6(bytes);
  }

  friend std::istream &operator>>(std::istream &is, ipv6_header &header) {
    is.read(reinterpret_cast<char *>(header.rep_), 40);
    if (header.version() != 6)
      is.setstate(std::ios::failbit);
    return is;
  }

private:
  unsigned char rep_[40];
};
} // namespace net

#endif // IPV6_HEADER_HPP

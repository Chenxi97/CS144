#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto it = _arp_cache.find(next_hop_ip);
    if (it == _arp_cache.end()) {
        // can not find mapping, send ARP broadcast message
        auto pending_it = _pending_dgrams.find(next_hop_ip);
        if (pending_it != _pending_dgrams.end()) {
            pending_it->second.dgrams.emplace_back(dgram);
            if (pending_it->second.last_send_time + 5000 >= _current_time) {
                // do not sent arp message if have sent in last 5 seconds
                return;
            }
            pending_it->second.last_send_time = _current_time;
        } else {
            _pending_dgrams.insert_or_assign(next_hop_ip, PendingDgrams{_current_time, dgram});
        }
        send_arp(ARPMessage::OPCODE_REQUEST, ETHERNET_BROADCAST, next_hop_ip);
    } else {
        // found mapping
        send_ipv4(dgram, it->second.ethernet_address);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram{};
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp{};
        if (arp.parse(frame.payload()) == ParseResult::NoError) {
            // update arp cache
            _arp_cache.insert_or_assign(arp.sender_ip_address, ARPCache{_current_time, arp.sender_ethernet_address});
            if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) {
                send_arp(ARPMessage::OPCODE_REPLY, arp.sender_ethernet_address, arp.sender_ip_address);
            }
            // send pending internet datagrams
            auto it = _pending_dgrams.find(arp.sender_ip_address);
            if (it != _pending_dgrams.end()) {
                for (auto dgram : it->second.dgrams) {
                    send_ipv4(dgram, arp.sender_ethernet_address);
                }
                _pending_dgrams.erase(it);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;
    for (auto it = _arp_cache.begin(); it != _arp_cache.end();) {
        if (it->second.create_time + 30000 < _current_time) {
            it = _arp_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkInterface::send_arp(const uint16_t opcode,
                                const EthernetAddress target_ethernet,
                                const uint32_t target_ip) {
    EthernetFrame frame{};
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = target_ethernet;
    {
        ARPMessage arp{};
        arp.opcode = opcode;
        arp.sender_ethernet_address = _ethernet_address;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        if (target_ethernet != ETHERNET_BROADCAST) {
            arp.target_ethernet_address = target_ethernet;
        }
        arp.target_ip_address = target_ip;
        frame.payload() = arp.serialize();
    }
    _frames_out.emplace(frame);
}

void NetworkInterface::send_ipv4(const InternetDatagram &dgram, const EthernetAddress &dst) {
    EthernetFrame frame{};
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.header().dst = dst;
    frame.payload() = dgram.serialize();
    _frames_out.emplace(frame);
}

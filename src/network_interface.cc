#include <cstdint>
#include <iostream>
#include <utility>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::send_ipdatagram( const InternetDatagram& dgram, const uint32_t next_hop )
{
  EthernetFrame ethframe;
  ethframe.header.src = ethernet_address_;
  ethframe.header.dst = ipAddrToEther_[next_hop];
  ethframe.header.type = EthernetHeader::TYPE_IPv4;
  ethframe.payload = serialize( dgram );
  port_->transmit( *this, ethframe );
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  (void)dgram;
  (void)next_hop;
  if ( ipAddrToEther_.contains( next_hop.ipv4_numeric() ) ) {
    send_ipdatagram( dgram, next_hop.ipv4_numeric() );
  } else if(not waitedDatagrams_lives_.contains(next_hop.ipv4_numeric())) {

    // 制作arp 消息
    ARPMessage arp_message;
    arp_message.opcode = ARPMessage::OPCODE_REQUEST;
    arp_message.sender_ip_address = ip_address_.ipv4_numeric();
    arp_message.sender_ethernet_address = ethernet_address_;

    arp_message.target_ip_address = next_hop.ipv4_numeric();
    //arp_message.target_ethernet_address = ETHERNET_BROADCAST;

    EthernetFrame ethframe;
    ethframe.header.src = ethernet_address_;
    ethframe.header.dst = ETHERNET_BROADCAST;
    ethframe.header.type = EthernetHeader::TYPE_ARP;
    ethframe.payload = serialize( arp_message );

    ip_datagram_waited[next_hop.ipv4_numeric()] = dgram;
    waitedDatagrams_lives_[next_hop.ipv4_numeric()]=0;

    port_->transmit( *this, ethframe );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram datagram;
    parse(datagram, frame.payload);
    datagrams_received_.push( move( datagram ) );
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage receive_arpmes;
    parse(receive_arpmes, frame.payload);
    //?? 如果是收到缓存表里已有的呢？
    ipAddrToEther_[receive_arpmes.sender_ip_address] = receive_arpmes.sender_ethernet_address;
    ipToeth_lives_times_[receive_arpmes.sender_ip_address] = 0;

    if ( receive_arpmes.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( receive_arpmes.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage sendself_mes;
        sendself_mes.opcode = ARPMessage::OPCODE_REPLY;
        sendself_mes.sender_ip_address = ip_address_.ipv4_numeric();
        sendself_mes.sender_ethernet_address = ethernet_address_;

        sendself_mes.target_ip_address = receive_arpmes.sender_ip_address;
        sendself_mes.target_ethernet_address = receive_arpmes.sender_ethernet_address;

        EthernetFrame ethframe;
        ethframe.header.src = ethernet_address_;
        ethframe.header.dst = receive_arpmes.sender_ethernet_address;
        ethframe.header.type = EthernetHeader::TYPE_ARP;
        ethframe.payload = serialize( sendself_mes);
        port_->transmit(*this, ethframe);
      }
    } else if ( receive_arpmes.opcode == ARPMessage::OPCODE_REPLY ) {
      send_ipdatagram(ip_datagram_waited[receive_arpmes.sender_ip_address], receive_arpmes.sender_ip_address);
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  const uint64_t live_limit_ms = 30 * 1000; // 30s
  const uint64_t wait_limit_ms = 5*1000; // 5s

  time_ms_ += ms_since_last_tick;

  for ( auto iter = ipToeth_lives_times_.begin(); iter != ipToeth_lives_times_.end(); ) {
    iter->second += ms_since_last_tick;
    if ( iter->second >= live_limit_ms ) {
      ipAddrToEther_.erase(iter->first);
      iter = ipToeth_lives_times_.erase( iter);      
    } else {
      iter++;
    }
  }
  
  for ( auto iter = waitedDatagrams_lives_.begin(); iter != waitedDatagrams_lives_.end(); ) {
    iter->second += ms_since_last_tick;
    if ( iter->second >= wait_limit_ms ) {
      iter = waitedDatagrams_lives_.erase( iter );
    } else {
      iter++;
    }
  }
}

#include "router.hh"
#include "address.hh"

#include <bitset>
#include <cstdint>
#include <iostream>
#include <queue>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  // Your code here.
  _forward_table.insert( { { prefix_length, route_prefix }, { next_hop, interface_num } } );
}

uint32_t make_mask( uint8_t len )
{
  uint32_t mask = 0;
  uint32_t offset = 31;
  while ( len-- ) {
    mask = mask | ( 1 << offset );
    offset--;
  }
  return mask;
}
string ipinfo( uint32_t addr )
{
  return Address::from_ipv4_numeric( addr ).ip();
}
// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& interface : _interfaces ) {
    auto& que_datagram = interface->datagrams_received();
    while ( not que_datagram.empty() ) {
      auto datagram = que_datagram.front();
      que_datagram.pop();
      if ( datagram.header.ttl <= 1 ) {
        continue;
      }
      datagram.header.ttl--;

      for ( auto& netid_hopinfo : _forward_table ) {
        uint32_t rawip = datagram.header.dst;

        uint32_t target_net = rawip & make_mask( netid_hopinfo.first.first );
        // cerr << "net:" << ipinfo( target_net ) << "\n";
        // cerr<<"mask: "<<bitset<32>(make_mask(netid_hopinfo.first.first))<<"\n";
        // cerr<<(uint32_t)netid_hopinfo.first.first<<" ---  "<<netid_hopinfo.first.second<<"\n";
        if ( target_net == netid_hopinfo.first.second ) {
          auto& hop_info = netid_hopinfo.second;
          Address addr
            = ( hop_info.first.has_value() ? hop_info.first.value() : Address::from_ipv4_numeric( rawip ) );

          // cerr<<target_net<< "  "<<addr.ip()<<"\n\n";
          _interfaces.at( hop_info.second )->send_datagram( datagram, addr );
          break;
        }
      }
    }
  }
  // Your code here.
}

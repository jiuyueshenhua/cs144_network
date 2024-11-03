#pragma once

#include "ethernet_header.hh"
#include "parser.hh"

#include <vector>

struct EthernetFrame
{
  EthernetHeader header {};
  std::vector<std::string> payload {};

  void parse( Parser& parser )//? 如何使用该方法？
  {
    header.parse( parser );
    parser.all_remaining( payload );
  }

  //?  如何使用该方法？
  void serialize( Serializer& serializer ) const
  {
    header.serialize( serializer );
    serializer.buffer( payload );
  }
  //* [dst,src,tytp],p[0],p[1].....
};

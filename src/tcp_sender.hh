#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {
    RTO_ms_ = initial_RTO_ms;
  }

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );
  //? 个人感觉非常突兀，重传操作居然在定时这里

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }
  //* 复制,析构 ??
private:
  // Variables initialized in constructor
  ByteStream input_; //* 源于应用程序的流
  Wrap32 isn_;       //! Wrap32
  uint64_t initial_RTO_ms_ {};

  uint32_t last_win_size_ { 1 };
  uint64_t send_absSeqno_ {}; // 下一个待发送的绝对序列号
  uint64_t ack_absSeqno_ {};  // 下一个待确认的序列号
  uint64_t RTO_ms_ {};
   bool IsWindowEmpty { false };
//   bool EmptySended {false};
  bool fined {false};

  struct
  {
    std::string buf_;
    // std::map<Wrap32, uint64_t> segmentHead_timer;
    std::map<uint64_t, uint64_t> segmentHead_timer;
    std::unordered_map<Wrap32, uint64_t> SeqnoToAbsSeqno;

    uint64_t retransmit_num { 0 };
    uint64_t nextsend_id { 0 };
    // outstanding_cache_() : buf_(), segmentHead_timer(), retransmit_num( 0 ), nextsend_id( 0 ) {}
  } outstanding_cache_;
  
};
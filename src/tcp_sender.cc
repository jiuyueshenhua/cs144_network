#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <utility>

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return send_absSeqno_ - ack_absSeqno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return outstanding_cache_.retransmit_num;
}

void TCPSender::push( const TransmitFunction& transmit )
{

  // Your code here.
  if ( input_.has_error() ) {
    // !app->Sender的流有意外，在Send->Rec 的流发生该包
    TCPSenderMessage mes;
    mes.seqno = Wrap32::wrap( send_absSeqno_, isn_ );
    mes.RST = true;
    transmit( mes );
    return;
  }

  if ( fined ) { //! 在push里，只发送没有发送过的数据
    return;
  }

  if ( !input_.reader().is_finished() ) {
    outstanding_cache_.buf_.append( input_.reader().peek() );
    input_.reader().pop( input_.reader().peek().size() );
  }

  //   if ( IsWindowEmpty && !EmptySended ) {
  //     last_win_size_ = 1;
  //     EmptySended = true;
  //   }

  // std::set<TCPSenderMessage> meses;
  bool hasSYN = send_absSeqno_ == 0;
  bool hasFin = outstanding_cache_.nextsend_id == outstanding_cache_.buf_.size() && input_.reader().is_finished()
                && last_win_size_;
  bool hasData = outstanding_cache_.nextsend_id < outstanding_cache_.buf_.size() && last_win_size_ > 0;

  //   std::cout << "sz: " << last_win_size_ << " data " << hasData << "\n";
  //   std::cout << outstanding_cache_.buf_ << "\n";

  // 发送数据报
  while ( hasData || hasSYN || hasFin ) {

    
    size_t pay_sz
      = std::min( outstanding_cache_.buf_.size() - outstanding_cache_.nextsend_id, TCPConfig::MAX_PAYLOAD_SIZE );
    pay_sz = std::min( pay_sz, static_cast<size_t>( last_win_size_ ) );
    // 加载到数据段
    // 这里的区间用 head，size表示
    TCPSenderMessage smes;    
    smes.payload = outstanding_cache_.buf_.substr( outstanding_cache_.nextsend_id, pay_sz );
    smes.seqno = Wrap32::wrap( send_absSeqno_, isn_ );

    outstanding_cache_.nextsend_id += pay_sz;
    last_win_size_ -= pay_sz;

    outstanding_cache_.segmentHead_timer[send_absSeqno_] = 0; // 存储端头
    outstanding_cache_.SeqnoToAbsSeqno[smes.seqno] = send_absSeqno_;

    // 考虑特殊情况，移动seqno
    if ( hasSYN ) {
      smes.SYN = true;
      send_absSeqno_++;
      hasSYN = false;
      last_win_size_--;
    }

    send_absSeqno_ += pay_sz;

    hasFin = outstanding_cache_.nextsend_id == outstanding_cache_.buf_.size() && input_.reader().is_finished()
             && last_win_size_;
    if ( hasFin ) {
      smes.FIN = true;
      send_absSeqno_++;
      hasFin = false;
      last_win_size_--;
      fined = true;
    }
    hasData = outstanding_cache_.nextsend_id < outstanding_cache_.buf_.size() && last_win_size_ > 0;

    transmit( smes );
  }
  //   if ( IsWindowEmpty )
  //     last_win_size_ = 0;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage mes {};
  mes.seqno = Wrap32::wrap( send_absSeqno_, isn_ );
  mes.RST = input_.has_error();
  return mes;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    //? 提醒app不必再输入数据了？
    input_.set_error();
    return;
  }
  // 检查合规包，再按接受到合规包后的规则执行
  //  TCPConfig::MAX_PAYLOAD_SIZE;
  //   Your code here.

  IsWindowEmpty = msg.window_size == 0;
  //   EmptySended = false;
  //! 注意有先收到包，再发送syn的情况
  if ( !msg.ackno.has_value() ) {
    last_win_size_ = msg.window_size;
    return;
  }

  bool valid = ( outstanding_cache_.SeqnoToAbsSeqno.contains( msg.ackno.value() ) )
               || msg.ackno->unwrap( isn_, send_absSeqno_ ) == send_absSeqno_;
  valid = valid && msg.ackno.value() != Wrap32::wrap( outstanding_cache_.segmentHead_timer.begin()->first, isn_ );
  if ( !valid ) {
    int64_t remain = Wrap32::wrap( send_absSeqno_, isn_ ).diff( msg.ackno.value() + msg.window_size, isn_ );
    last_win_size_ = std::max( static_cast<int64_t>( 0 ), remain );
    return;
  }

  // 删除buf的已被确认的段
  if ( outstanding_cache_.segmentHead_timer.size() != 0 ) {
    uint64_t ack_absseqno {};
    if ( outstanding_cache_.SeqnoToAbsSeqno.contains( msg.ackno.value() ) ) {
      ack_absseqno = outstanding_cache_.SeqnoToAbsSeqno[msg.ackno.value()];
    } else {
      ack_absseqno = send_absSeqno_;
    }

    auto sz = ack_absseqno - ack_absSeqno_;
    if ( ack_absSeqno_ == 0 ) {
      sz--;
    }

    outstanding_cache_.buf_.erase( 0, sz );
    outstanding_cache_.nextsend_id -= sz;

    std::erase_if( outstanding_cache_.segmentHead_timer,
                   [&ack_absseqno]( const auto& pair ) { return pair.first < ack_absseqno; } );

    auto pred = [&ack_absseqno, this]( const std::pair<Wrap32, uint64_t>& pair ) {
      return pair.first.lessThan( Wrap32::wrap( ack_absseqno, isn_ ), isn_ );
    };
    std::erase_if( outstanding_cache_.SeqnoToAbsSeqno, pred );
    ack_absSeqno_ = ack_absseqno;
  }

  RTO_ms_ = initial_RTO_ms_;
  outstanding_cache_.retransmit_num = 0;

  for ( auto& p : outstanding_cache_.segmentHead_timer ) {
    p.second = 0;
  }

  last_win_size_ = std::max( msg.window_size, static_cast<uint16_t>( 1 ) );
  // 遇到大小为0的窗口，按大小为1来处理
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.

  if ( outstanding_cache_.segmentHead_timer.size() == 0 )
    return;

  for ( auto& p : outstanding_cache_.segmentHead_timer ) {
    p.second += ms_since_last_tick;
    // std::cout<<p.first<<"  "<<p.second<<"\n";
  }

  auto fir_iter = outstanding_cache_.segmentHead_timer.begin();
  if ( fir_iter->second >= RTO_ms_ ) {
    TCPSenderMessage mes;

    mes.seqno = Wrap32::wrap( fir_iter->first, isn_ );

    int64_t sz = 0;
    if ( ack_absSeqno_ == 0 ) {
      mes.SYN = true;
      sz--;
    }

    if ( outstanding_cache_.segmentHead_timer.size() != 1 ) {
      auto sec_iter = fir_iter;
      sec_iter++;
      sz += sec_iter->first - ack_absSeqno_;

    } else {
      sz += outstanding_cache_.nextsend_id;
    }
    mes.payload = outstanding_cache_.buf_.substr( 0, sz );

    bool hasFin = ack_absSeqno_ + sz + 1 == send_absSeqno_ && input_.reader().is_finished();
    if ( hasFin && outstanding_cache_.segmentHead_timer.size() == 1 ) {
      mes.FIN = true;
    }
    transmit( mes );

    if ( !IsWindowEmpty ) {
      RTO_ms_ *= 2;
    }

    outstanding_cache_.retransmit_num++;
    fir_iter->second = 0;
  }
}

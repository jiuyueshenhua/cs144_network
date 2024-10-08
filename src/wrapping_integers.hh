#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
/*
 * The Wrap32 type represents a 32-bit unsigned integer that:
 *    - starts at an arbitrary "zero point" (initial value), and
 *    - wraps back to zero when it reaches 2^32 - 1.
 */

class Wrap32
{

public:
  Wrap32() = default;
  explicit Wrap32( uint32_t raw_value ) : raw_value_( raw_value ) {}

  /* Construct a Wrap32 given an absolute sequence number n and the zero point. */
  static Wrap32 wrap( uint64_t n, Wrap32 zero_point );
  // static size_t Hash( const Wrap32& w ) { return std::hash<uint32_t> {}( w.raw_value_ ); }

  /*
   * The unwrap method returns an absolute sequence number that wraps to this Wrap32, given the zero point
   * and a "checkpoint": another absolute sequence number near the desired answer.
   *
   * There are many possible absolute sequence numbers that all wrap to the same Wrap32.
   * The unwrap method should return the one that is closest to the checkpoint.
   */
  uint64_t unwrap( Wrap32 zero_point, uint64_t checkpoint ) const;

  Wrap32 operator+( uint32_t n ) const { return Wrap32 { raw_value_ + n }; }
  bool operator==( const Wrap32& other ) const { return raw_value_ == other.raw_value_; }
  bool lessThan( const Wrap32& other, const Wrap32& ISN ) const
  {
    uint64_t base = 1LL << 32;
    uint32_t offseta = ( raw_value_ - ISN.raw_value_ + base ) % base;
    uint32_t offsetb = ( other.raw_value_ - ISN.raw_value_ + base ) % base;
    return offseta < offsetb;
  }
  int64_t diff( const Wrap32& other, const Wrap32& ISN ) const
  {
    uint64_t base = 1LL << 32;
    uint32_t offseta = ( raw_value_ - ISN.raw_value_ + base ) % base;
    uint32_t offsetb = ( other.raw_value_ - ISN.raw_value_ + base ) % base;
    return offsetb - offseta;
  }

protected:
  uint32_t raw_value_ {};
  friend class std::hash<Wrap32>;
};

template<>
class std::hash<Wrap32>
{ // 偏特化（这里使用了标准库已经提供的hash偏特化类hash<string>，hash<int>()）
public:
  size_t operator()( const Wrap32& w ) const { return std::hash<uint32_t> {}( w.raw_value_ ); }
};

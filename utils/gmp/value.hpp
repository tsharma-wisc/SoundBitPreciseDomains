#ifndef utils_gmp_value_hpp
#define utils_gmp_value_hpp

#include <iostream>

// Just get the fixed-width types using stdint
// TODO: Bad because stdint is deprecated, but this is a quick fix.
#include <stdint.h>
typedef uint64_t LONG_UINT;
typedef uint32_t UINT;
typedef uint16_t USHORT;
typedef uint8_t  UCHAR;
typedef int64_t  LONG_INT;
typedef int32_t  INT;
typedef int16_t  SHORT;
typedef int8_t   CHAR;
typedef bool UINT1;
////////////////////////////////////////////////////////////

namespace utils {

enum Bitsize {sixty_four = 64, thirty_two = 32, sixteen = 16, eight = 8, one = 1};

LONG_UINT GetBitWidth(Bitsize b);

Bitsize GetBitsize(LONG_UINT w);

class Value {
public:
  // Default empty constructor (TODO: Delete as this will create bitsize issues in future)
  Value(const Bitsize size = thirty_two);
  //Value(const LONG_UINT val);
  Value(const Bitsize size, const LONG_UINT val);
  ~Value();

  const Value& operator+() const;
  const Value operator-() const;
  const Value operator~() const;

  bool is_zero() const;
  const Value operator+(const Value& v) const;
  const Value operator-(const Value& v) const;
  const Value operator*(const Value& v) const;
  const Value operator%(const Value& v) const;

  // Signed modulo operation which interprets this and v as signed
  const Value smod(const Value& v) const;
  const Value operator/(const Value& v) const;

  // Signed division operation which interprets this and v as signed
  const Value sdiv(const Value& v) const;
  const Value operator&(const Value& v) const;
  const Value operator|(const Value& v) const;
  const Value operator^(const Value& v) const;
  const Value operator>>(const Value& v) const;
  const Value operator<<(const Value& v) const;
  bool operator< (const Value& v) const;
  bool operator> (const Value& v) const;
  bool operator<= (const Value& v) const;
  bool operator>= (const Value& v) const;
  bool operator== (const Value& v) const;
  bool operator!= (const Value& v) const;
  Value& operator=(const Value& v);
  Value ZeroExtend(Bitsize new_bitsize) const;
  Value SignExtend(Bitsize new_bitsize) const;
  Value Trunc(Bitsize new_bitsize) const;
  Value SignExtendOrTrunc(Bitsize new_bitsize) const;

  Bitsize GetBitsize() const;

  Bitsize size_;
  LONG_UINT bv64_;
  UINT bv32_;
  USHORT bv16_;
  UCHAR bv8_;
  UINT1 bv1_;
};

size_t compute_value_rank(Value v);
Value multiplicative_inverse_value(Value a);

size_t GetMinPosOrNegPopCount(Value v);

std::ostream& operator<<(std::ostream& o, const Value v);

} //namespace utils

#endif // utils_gmp_value_hpp

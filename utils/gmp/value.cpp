#include "value.hpp"
#include <cassert>

namespace utils {

  LONG_UINT GetBitWidth(Bitsize b) {
    switch(b)
      {
      case sixty_four:
        return 64ull;
      case thirty_two:
        return 32ull;
      case sixteen:
        return 16ull;
      case eight:
        return 8ull;
      case one:
        return 1ull;
      }

    assert(false);
    return 0ull;
  }

  Bitsize GetBitsize(LONG_UINT w) {
    switch(w)
      {
      case 64:
        return sixty_four;
      case 32:
        return thirty_two;
      case 16:
        return sixteen;
      case 8:
        return eight;
      case 1:
        return one;
      default:
        assert(false);
        return one;
      }
    return one;
  }

  // Default empty constructor
  Value::Value(const Bitsize size)
    : size_(size)
  {
  }
 
  // Value::Value(const LONG_UINT val)
  //   : size_(thirty_two)
  // {
  //   bv32_ = (UINT)val;
  // }

  Value::Value(const Bitsize size, const LONG_UINT val)
    : size_(size)
  {
    switch(size_)
      {
      case sixty_four:
        bv64_ = val;
        break;
      case thirty_two:
        bv32_ = (UINT)val;
        break;
      case sixteen:
        bv16_ = (USHORT)val;
        break;
      case eight:
        bv8_ = (UCHAR)val;
        break;
      case one:
        bv1_ = val%2;
      }
  }

  Value::~Value() {}

  const Value& Value::operator+() const { return *this; }

  const Value Value::operator-() const 
  { 
    switch(size_)
      {
      case sixty_four:
        return Value(size_, -bv64_);
      case thirty_two:
        return Value(size_, -bv32_);
      case sixteen:
        return Value(size_, -bv16_);
      case eight:
        return Value(size_, -bv8_);
      case one:
        return Value(size_, -bv1_);
      }
    assert(false);
    return *this;
  }

  const Value Value::operator~() const 
  { 
    switch(size_)
      {
      case sixty_four:
        return Value(size_, ~bv64_);
      case thirty_two:
        return Value(size_, ~bv32_);
      case sixteen:
        return Value(size_, ~bv16_);
      case eight:
        return Value(size_, ~bv8_);
      case one:
        return Value(size_, ~bv1_);
      }
    assert(false);
    return *this;
  }

  bool Value::is_zero() const 
  {
    switch(size_)
      {
      case sixty_four:
        return (bv64_ == 0);
      case thirty_two:
        return (bv32_ == 0);
      case sixteen:
        return (bv16_ == 0);
      case eight:
        return (bv8_ == 0);
      case one:
        return (bv1_ == 0);
      }
    assert(false);
    return false;
  }

  const Value Value::operator+(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_+v.bv64_);
      case thirty_two:
        return Value(size_, bv32_+v.bv32_);
      case sixteen:
        return Value(size_, bv16_+v.bv16_);
      case eight:
        return Value(size_, bv8_+v.bv8_);
      case one:
        return Value(size_, bv1_+v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator-(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_-v.bv64_);
      case thirty_two:
        return Value(size_, bv32_-v.bv32_);
      case sixteen:
        return Value(size_, bv16_-v.bv16_);
      case eight:
        return Value(size_, bv8_-v.bv8_);
      case one:
        return Value(size_, bv1_-v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator*(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_*v.bv64_);
      case thirty_two:
        return Value(size_, bv32_*v.bv32_);
      case sixteen:
        return Value(size_, bv16_*v.bv16_);
      case eight:
        return Value(size_, bv8_*v.bv8_);
      case one:
        return Value(size_, bv1_*v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator%(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_%v.bv64_);
      case thirty_two:
        return Value(size_, bv32_%v.bv32_);
      case sixteen:
        return Value(size_, bv16_%v.bv16_);
      case eight:
        return Value(size_, bv8_%v.bv8_);
      case one:
        return Value(size_, bv1_%v.bv1_);
      }
    assert(false);
    return v;
  }

  // Signed modulo operation which interprets this and v as signed
  const Value Value::smod(const Value& v) const { 
    assert(size_ == v.size_);
    switch(size_) {
    case sixty_four: {
      LONG_INT num = bv64_;
      LONG_INT den = v.bv64_;
      LONG_INT smod = num%den;
      return Value(size_, LONG_UINT(smod));
    }
    case thirty_two: {
      INT num = bv32_;
      INT den = v.bv32_;
      INT smod = num%den;
      return Value(size_, UINT(smod));
    }
    case sixteen: {
      SHORT num = bv16_;
      SHORT den = v.bv16_;
      SHORT smod = num%den;
      return Value(size_, USHORT(smod));
    }
    case eight: {
      CHAR num = bv16_;
      CHAR den = v.bv16_;
      CHAR smod = num%den;
      return Value(size_, UCHAR(smod));
    }
    case one:
      return Value(size_, bv1_%v.bv1_);
    }
    assert(false);
    return v;
  }

  const Value Value::operator/(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_/v.bv64_);
      case thirty_two:
        return Value(size_, bv32_/v.bv32_);
      case sixteen:
        return Value(size_, bv16_/v.bv16_);
      case eight:
        return Value(size_, bv8_/v.bv8_);
      case one:
        return Value(size_, bv1_/v.bv1_);
      }
    assert(false);
    return v;
  }

  // Signed division operation which interprets this and v as signed
  const Value Value::sdiv(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four: {
        LONG_INT num = bv64_;
        LONG_INT den = v.bv64_;
        LONG_INT sdiv = num/den;
        return Value(size_, LONG_UINT(sdiv));
      }
      case thirty_two: {
        INT num = bv32_;
        INT den = v.bv32_;
        INT sdiv = num/den;
        return Value(size_, UINT(sdiv));
      }
      case sixteen: {
        SHORT num = bv16_;
        SHORT den = v.bv16_;
        SHORT sdiv = num/den;
        return Value(size_, USHORT(sdiv));
      }
      case eight: {
        CHAR num = bv16_;
        CHAR den = v.bv16_;
        CHAR sdiv = num/den;
        return Value(size_, UCHAR(sdiv));
      }
      case one:
        return Value(size_, bv1_/v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator&(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_&v.bv64_);
      case thirty_two:
        return Value(size_, bv32_&v.bv32_);
      case sixteen:
        return Value(size_, bv16_&v.bv16_);
      case eight:
        return Value(size_, bv8_&v.bv8_);
      case one:
        return Value(size_, bv1_&v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator|(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_|v.bv64_);
      case thirty_two:
        return Value(size_, bv32_|v.bv32_);
      case sixteen:
        return Value(size_, bv16_|v.bv16_);
      case eight:
        return Value(size_, bv8_|v.bv8_);
      case one:
        return Value(size_, bv1_|v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator^(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_^v.bv64_);
      case thirty_two:
        return Value(size_, bv32_^v.bv32_);
      case sixteen:
        return Value(size_, bv16_^v.bv16_);
      case eight:
        return Value(size_, bv8_^v.bv8_);
      case one:
        return Value(size_, bv1_^v.bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator>>(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_>>v.bv64_);
      case thirty_two:
        return Value(size_, bv32_>>v.bv32_);
      case sixteen:
        return Value(size_, bv16_>>v.bv16_);
      case eight:
        return Value(size_, bv8_>>v.bv8_);
      case one:
        if(v.bv1_== 1)
          return Value(size_, 0);
        else 
          return Value(size_, bv1_);
      }
    assert(false);
    return v;
  }

  const Value Value::operator<<(const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return Value(size_, bv64_<<v.bv64_);
      case thirty_two:
        return Value(size_, bv32_<<v.bv32_);
      case sixteen:
        return Value(size_, bv16_<<v.bv16_);
      case eight:
        return Value(size_, bv8_<<v.bv8_);
      case one:
        if(v.bv1_== 1) 
          return Value(size_, 0);
        else 
          return Value(size_, bv1_);
      }
    assert(false);
    return v;
  }

  bool Value::operator< (const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return bv64_<v.bv64_;
      case thirty_two:
        return bv32_<v.bv32_;
      case sixteen:
        return bv16_<v.bv16_;
      case eight:
        return bv8_<v.bv8_;
      case one:
        return bv1_<v.bv1_;
      }

    assert(false);
    return false;
  }

  bool Value::operator> (const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return bv64_>v.bv64_;
      case thirty_two:
        return bv32_>v.bv32_;
      case sixteen:
        return bv16_>v.bv16_;
      case eight:
        return bv8_>v.bv8_;
      case one:
        return bv1_>v.bv1_;
      }

    assert(false);
    return false;
  }

  bool Value::operator<= (const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return bv64_<=v.bv64_;
      case thirty_two:
        return bv32_<=v.bv32_;
      case sixteen:
        return bv16_<=v.bv16_;
      case eight:
        return bv8_<=v.bv8_;
      case one:
        return bv1_<=v.bv1_;
      }

    assert(false);
    return false;
  }

  bool Value::operator>= (const Value& v) const 
  { 
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return bv64_>=v.bv64_;
      case thirty_two:
        return bv32_>=v.bv32_;
      case sixteen:
        return bv16_>=v.bv16_;
      case eight:
        return bv8_>=v.bv8_;
      case one:
        return bv1_>=v.bv1_;
      }

    assert(false);
    return false;
  }

  bool Value::operator== (const Value& v) const
  {
    assert(size_ == v.size_);
    switch(size_)
      {
      case sixty_four:
        return bv64_ == v.bv64_;
      case thirty_two:
        return bv32_ == v.bv32_;
      case sixteen:
        return bv16_ == v.bv16_;
      case eight:
        return bv8_ == v.bv8_;
      case one:
        return bv1_ == v.bv1_;
      }

    assert(false);
    return false;
  }

  bool Value::operator!= (const Value& v) const
  {
    return !(*this == v);
  }

  Value& Value::operator=(const Value& v)
  {
    if(this != &v) {
      size_ = v.size_;
      bv64_ = v.bv64_;
      bv32_ = v.bv32_;
      bv16_ = v.bv16_;
      bv8_ = v.bv8_;
      bv1_ = v.bv1_;
    }
    return *this;
  }

  Value Value::ZeroExtend(Bitsize new_bitsize) const 
  {
    LONG_UINT new_bitwidth = utils::GetBitWidth(new_bitsize);
    LONG_UINT old_bitwidth = utils::GetBitWidth(size_);
    assert(old_bitwidth <= new_bitwidth);
    switch(size_)
      {
      case sixty_four:
        return Value(new_bitsize, bv64_);
      case thirty_two:
        return Value(new_bitsize, LONG_UINT(bv32_));
      case sixteen:
        return Value(new_bitsize, LONG_UINT(bv16_));
      case eight:
        return Value(new_bitsize, LONG_UINT(bv8_));
      case one:
        return Value(new_bitsize, LONG_UINT(bv1_));
      }

    assert(false);

    return Value(new_bitsize);
  }
  
  Value Value::SignExtend(Bitsize new_bitsize) const
  {
    LONG_UINT new_bitwidth = utils::GetBitWidth(new_bitsize);
    LONG_UINT old_bitwidth = utils::GetBitWidth(size_);
    assert(old_bitwidth <= new_bitwidth);
    switch(size_)
      {
      case sixty_four:
        return Value(new_bitsize, bv64_);
      case thirty_two:
        return Value(new_bitsize, LONG_UINT(LONG_INT(INT(bv32_))));
      case sixteen:
        return Value(new_bitsize, LONG_UINT(LONG_INT(SHORT(bv16_))));
      case eight:
        return Value(new_bitsize, LONG_UINT(LONG_INT(CHAR(bv8_))));
      case one:
        return Value(new_bitsize, LONG_UINT(LONG_INT(bool(bv1_))));
      }

    assert(false);

    return Value(new_bitsize);
  }

  Value Value::Trunc(Bitsize new_bitsize) const
  {
    LONG_UINT new_bitwidth = utils::GetBitWidth(new_bitsize);
    LONG_UINT old_bitwidth = utils::GetBitWidth(size_);
    assert(old_bitwidth >= new_bitwidth);
    switch(size_)
      {
      case sixty_four:
        return Value(new_bitsize, bv64_);
      case thirty_two:
        return Value(new_bitsize, LONG_UINT(bv32_));
      case sixteen:
        return Value(new_bitsize, LONG_UINT(bv16_));
      case eight:
        return Value(new_bitsize, LONG_UINT(bv8_));
      case one:
        return Value(new_bitsize, LONG_UINT(bv1_));
      }

    assert(false);

    return Value(new_bitsize);
  }

  Value Value::SignExtendOrTrunc(Bitsize new_bitsize) const
  {
    LONG_UINT new_bitwidth = utils::GetBitWidth(new_bitsize);
    LONG_UINT old_bitwidth = utils::GetBitWidth(size_);
    if(old_bitwidth == new_bitwidth)
      return Value(*this);

    if(old_bitwidth < new_bitwidth)
      return SignExtend(new_bitsize);

    return Trunc(new_bitsize);
  }

  Bitsize Value::GetBitsize() const
  {
    return size_;
  }



  template <typename T>
  size_t highest_power()
  { return sizeof(T) * 8; }

  //
  // compute_rank
  //
  // Version of Fig. 5-14 from Hacker's Delight that uses a loop
  // so that it can be parameterized on highest_power
  template <typename T>
  inline size_t compute_rank(T x) {
    T y;
    size_t n;

    if (x == T(0)) return highest_power<T>();

    n = highest_power<T>() - 1;
    for (size_t shift = highest_power<T>()/2; shift != 0; shift /= 2) {
      y = x << shift;
      if (! (y == T(0)) ) {
        n = n - shift;
        x = y;
      }
    }
    return n;
  }

  size_t compute_value_rank(Value v) 
  {
    switch(v.size_)
      {
      case sixty_four:
        return compute_rank(v.bv64_);
      case thirty_two:
        return compute_rank(v.bv32_);
      case sixteen:
        return compute_rank(v.bv16_);
      case eight:
        return compute_rank(v.bv8_);
      case one:
        return compute_rank(v.bv1_);
      }
    assert(false);
    return 0;
  }


  // find the multiplicative inverse
  template <typename T>
  T multiplicative_inverse(T d) {
    T xn ,t;
    if(d % 2 != 1)
      assert(false);

    xn = d;
    while(true) {
      t = d*xn;
      if(t == 1) return xn;
      xn = xn*((T)2-t);
    }
  }
    
  // A template specialization of multiplicative_inverse for BV1
  //
  template <>
  inline UINT1 multiplicative_inverse(UINT1 d) {
    if(d != 1)
      assert(false);
    return d;
  }

  Value multiplicative_inverse_value(Value v)
  {
    switch(v.size_) {
      case sixty_four:
        return Value(v.size_, multiplicative_inverse(v.bv64_));
      case thirty_two:
        return Value(v.size_, multiplicative_inverse(v.bv32_));
      case sixteen:
        return Value(v.size_, multiplicative_inverse(v.bv16_));
      case eight:
        return Value(v.size_, multiplicative_inverse(v.bv8_));
      case one:
        return Value(v.size_, multiplicative_inverse(v.bv1_));
      }
    assert(false);
    return Value(thirty_two, 0);
  }

  std::ostream& operator<<(std::ostream& o, const Value v)
  {
    switch(v.size_)
      {
      case sixty_four:
        o << v.bv64_;
        break;
      case thirty_two:
        o << v.bv32_;
        break;
      case sixteen:
        o << v.bv16_;
        break;
      case eight:
        o << v.bv8_;
        break;
      case one:
        o << v.bv1_;
      }
    return o;
  }


  // Function to calculate population count
  template <typename T>
  size_t pop_count(T a) { assert(false); return 0; }

  // Specializations of pop_count operations
  template <>
  inline size_t pop_count(LONG_UINT data) {
    LONG_UINT p = data;
    p = (p & 0x5555555555555555ULL) + ((p >> 1) & 0x5555555555555555ULL);
    p = (p & 0x3333333333333333ULL) + ((p >> 2) & 0x3333333333333333ULL);
    p = (p & 0x0F0F0F0F0F0F0F0FULL) + ((p >> 4) & 0x0F0F0F0F0F0F0F0FULL);
    p = (p & 0x00FF00FF00FF00FFULL) + ((p >> 8) & 0x00FF00FF00FF00FFULL);
    p = (p & 0x0000FFFF0000FFFFULL) + ((p >>16) & 0x0000FFFF0000FFFFULL);
    p = (p & 0x00000000FFFFFFFFULL) + ((p >>32) & 0x00000000FFFFFFFFULL);
    return (size_t)p;
  }

  template <>
  inline size_t pop_count(UINT data) {
    UINT p = data;
    p = (p & 0x55555555) + ((p >> 1) & 0x55555555);
    p = (p & 0x33333333) + ((p >> 2) & 0x33333333);
    p = (p & 0x0F0F0F0F) + ((p >> 4) & 0x0F0F0F0F);
    p = (p & 0x00FF00FF) + ((p >> 8) & 0x00FF00FF);
    p = (p & 0x0000FFFF) + ((p >>16) & 0x0000FFFF);
    return (size_t)p;
  }

  template <>
  inline size_t pop_count(USHORT data) {
    USHORT p = data;
    p = (p & 0x5555) + ((p >> 1) & 0x5555);
    p = (p & 0x3333) + ((p >> 2) & 0x3333);
    p = (p & 0x0F0F) + ((p >> 4) & 0x0F0F);
    p = (p & 0x00FF) + ((p >> 8) & 0x00FF);
    return (size_t)p;
  }

  template <>
  inline size_t pop_count(UCHAR data) {
    UCHAR p = data;
    p = (p & 0x55) + ((p >> 1) & 0x55);
    p = (p & 0x33) + ((p >> 2) & 0x33);
    p = (p & 0x0F) + ((p >> 4) & 0x0F);
    return (size_t)p;
  }

  // Return the min pop count of val and -val. This is useful to figure out how 
  // complex the mutiplication with val would be
  template <typename T>
  size_t GetMinPosOrNegPopCount(T val)
  {
    T minus_val = -val;
    return (std::min) (pop_count(val), pop_count(minus_val)); 
  }

  size_t GetMinPosOrNegPopCount(Value v) 
  {
    switch(v.size_)
      {
      case sixty_four:
        return GetMinPosOrNegPopCount(v.bv64_);
      case thirty_two:
        return GetMinPosOrNegPopCount(v.bv32_);
      case sixteen:
        return GetMinPosOrNegPopCount(v.bv16_);
      case eight:
        return GetMinPosOrNegPopCount(v.bv8_);
      case one:
        return GetMinPosOrNegPopCount(v.bv1_);
      }
    assert(false);
    return 0;
  }

}

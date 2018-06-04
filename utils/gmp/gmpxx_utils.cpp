#include "gmpxx_utils.hpp"

namespace utils {
  // 64-bit get and set functions for gmp mpz class 
  // picked up from http://stackoverflow.com/questions/6248723/mpz-t-to-unsigned-long-long-conversion-gmp-lib
  mpz_class mpz_set_sll(LONG_INT sll) {
    mpz_t n;
    mpz_init(n);
    mpz_set_si(n, (int)(sll >> 32));     /* n = (int)sll >> 32 */
    mpz_mul_2exp(n, n, 32 );             /* n <<= 32 */
    mpz_add_ui(n, n, (unsigned int)sll); /* n += (unsigned int)sll */
    mpz_class ret(n);
    mpz_clear(n); // Free memory
    return ret;
  }

  mpz_class mpz_set_ull(LONG_UINT ull) {
    mpz_t n;
    mpz_init(n);
    mpz_set_ui(n, (unsigned int)(ull >> 32)); /* n = (unsigned int)(ull >> 32) */
    mpz_mul_2exp(n, n, 32);                   /* n <<= 32 */
    mpz_add_ui(n, n, (unsigned int)ull);      /* n += (unsigned int)ull */
    mpz_class ret(n);
    mpz_clear(n); // Free memory
    return ret;
  }

  LONG_UINT mpz_get_ull(mpz_class n) {
    LONG_UINT lo, hi;
    mpz_t tmp;

    mpz_init( tmp );
    mpz_mod_2exp( tmp, n.get_mpz_t(), 64 );   /* tmp = (lower 64 bits of n) */

    lo = mpz_get_ui( tmp );       /* lo = tmp & 0xffffffff */ 
    mpz_div_2exp( tmp, tmp, 32 ); /* tmp >>= 32 */
    hi = mpz_get_ui( tmp );       /* hi = tmp & 0xffffffff */

    mpz_clear( tmp );

    return (((LONG_UINT)hi) << 32) + lo;
  }

  LONG_INT mpz_get_sll(mpz_class n) {
    return (LONG_INT)mpz_get_ull(n); /* just use unsigned version */
  }

  mpz_class convert_to_mpz(Value c, bool is_signed) {
    mpz_t ret_mpz_t;
    mpz_init(ret_mpz_t);

    switch(c.GetBitsize())
      {
      case sixty_four:
        mpz_clear(ret_mpz_t); // Free memory
  
        // 64 bits need special function as GMP does not provide that interface by default
        return is_signed? mpz_set_sll((LONG_INT)c.bv64_): mpz_set_ull((LONG_UINT)c.bv64_);
        break;
      case thirty_two:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (INT)c.bv32_);
        } else {
          mpz_set_ui(ret_mpz_t, (UINT)c.bv32_);
        }
        break;
      case sixteen:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (INT)(SHORT)(c.bv16_));
        } else {
          mpz_set_ui(ret_mpz_t, (UINT)(USHORT)(c.bv16_));
        }
        break;
      case eight:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (INT)(CHAR)(c.bv8_));
        } else {
          mpz_set_ui(ret_mpz_t, (UINT)(UCHAR)(c.bv8_));
        }
        break;
      case one:
        mpz_set_si(ret_mpz_t, (INT)(c.bv1_));
        break;
      }

    mpz_class ret(ret_mpz_t);
    mpz_clear(ret_mpz_t); // Free memory
    return ret;
  }

  Value convert_to_value(mpz_class c, Bitsize b, bool is_signed) {
    switch(b) {
    case sixty_four: {
      LONG_UINT val;
      if(is_signed)
        val = (LONG_UINT)mpz_get_sll(c);
      else
        val = mpz_get_ull(c);
      return Value(b, val);
    }
    case thirty_two: {
      UINT val;
      if(is_signed)
        val = (UINT)mpz_get_si(c.get_mpz_t());
      else
        val = mpz_get_ui(c.get_mpz_t());
      return Value(b, val);
    }
    case sixteen: {
      USHORT val;
      if(is_signed)
        val = (USHORT)mpz_get_si(c.get_mpz_t());
      else
        val = (USHORT)mpz_get_ui(c.get_mpz_t());
      return Value(b, val);
    }
    case eight: {
      UCHAR val;
      if(is_signed)
        val = (UCHAR)mpz_get_si(c.get_mpz_t());
      else
        val = (UCHAR)mpz_get_ui(c.get_mpz_t());
      return Value(b, val);
    }
    case one: {
      UCHAR val;
      if(is_signed)
        val = (UCHAR)mpz_get_si(c.get_mpz_t());
      else
        val = (UCHAR)mpz_get_ui(c.get_mpz_t());
      return Value(b, val);
    }
    }
    return Value(b, 0ull);
  }

  Value GetMaxInt(Bitsize b, bool is_signed) {
    Value max;
    switch(b) {
    case sixty_four:
      max = is_signed? Value(b, 0x7fffffffffffffffll): Value(b, 0xffffffffffffffffull); 
      break;
    case thirty_two:
      max = is_signed? Value(b, 0x7fffffff)          : Value(b, 0xffffffffu); 
      break;
    case sixteen:
      max = is_signed? Value(b, 0x7fff)              : Value(b, 0xffffu); 
      break;
    case eight:
      max = is_signed? Value(b, 0x7f)                : Value(b, 0xffu); 
      break;
    case one:
      max = Value(b, 1); 
      break;
    }
    return max;
  }

  Value GetMinInt(Bitsize b, bool is_signed) {
    Value min;
    switch(b) {
    case sixty_four:
      min = is_signed? Value(b, 0x8000000000000000ll): Value(b, 0x0000000000000000ull); 
      break;
    case thirty_two:
      min = is_signed? Value(b, 0x80000000)          : Value(b, 0x00000000u); 
      break;
    case sixteen:
      min = is_signed? Value(b, 0x8000)              : Value(b, 0x0000u); 
      break;
    case eight:
      min = is_signed? Value(b, 0x80)                : Value(b, 0x00u); 
      break;
    case one:
      min = Value(b, 0); 
      break;
    }
    return min;
  }

  // mpz rshift and lshift utilities
  // Perform a >>a uint(b), where b is truncated to uint number
  mpz_class mpz_rshift_arith(const mpz_class a, const mpz_class b) {
    mpz_t c;
    mpz_init(c);
    mpz_fdiv_q_2exp(c, a.get_mpz_t(), mpz_get_ui(b.get_mpz_t()));
    mpz_class ret(c);
    mpz_clear(c); // Free memory
    return ret;
  }

  // Perform a << uint(b), where b is truncated to uint number
  mpz_class mpz_lshift(const mpz_class a, const mpz_class b) {
    mpz_t c;
    mpz_init(c);
    mpz_mul_2exp(c, a.get_mpz_t(), mpz_get_ui(b.get_mpz_t()));
    mpz_class ret(c);
    mpz_clear(c); // Free memory
    return ret;
  }

  // Logical functions
  mpz_class mpz_and(const mpz_class a, const mpz_class b) {
    mpz_t c;
    mpz_init(c);
    mpz_and(c, a.get_mpz_t(), b.get_mpz_t());
    mpz_class ret(c);
    mpz_clear(c); // Free memory
    return ret;
  }

  mpz_class mpz_or(const mpz_class a, const mpz_class b) {
    mpz_t c;
    mpz_init(c);
    mpz_ior(c, a.get_mpz_t(), b.get_mpz_t());
    mpz_class ret(c);
    mpz_clear(c); // Free memory
    return ret;
  }

  mpz_class mpz_not(const mpz_class a) {
    mpz_t com_a;
    mpz_init(com_a);
    mpz_com(com_a, a.get_mpz_t());
    mpz_class ret(com_a);
    mpz_clear(com_a); // Free memory
    return ret;
  }

  mpz_class mpz_xor(const mpz_class a, const mpz_class b) {
    mpz_class com_a = mpz_not(a);
    mpz_class com_b = mpz_not(b);
    mpz_class ret = mpz_or(mpz_and(a, com_b), mpz_and(com_a, b));
    return ret;
  }

}

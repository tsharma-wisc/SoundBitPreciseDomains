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

  mpz_class convert_to_mpz(utils::Value c, bool is_signed) {
    mpz_t ret_mpz_t;
    mpz_init(ret_mpz_t);

    switch(c.GetBitsize())
      {
      case utils::sixty_four:
        mpz_clear(ret_mpz_t); // Free memory
  
        // 64 bits need special function as GMP does not provide that interface by default
        return is_signed? mpz_set_sll((LONG_INT)(c.bv64_.get_value())): mpz_set_ull((LONG_UINT)(c.bv64_.get_value()));
        break;
      case utils::thirty_two:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (INT)(c.bv32_.get_value()));
        } else {
          mpz_set_ui(ret_mpz_t, (UINT)(c.bv32_.get_value()));
        }
        break;
      case utils::sixteen:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (long int)(SHORT)(c.bv16_.get_value()));
        } else {
          mpz_set_ui(ret_mpz_t, (long unsigned int)(USHORT)(c.bv16_.get_value()));
        }
        break;
      case utils::eight:
        if(is_signed) {
          mpz_set_si(ret_mpz_t, (long int)(CHAR)(c.bv8_.get_value()));
        } else {
          mpz_set_ui(ret_mpz_t, (long unsigned int)(UCHAR)(c.bv8_.get_value()));
        }
        break;
      case utils::one:
        mpz_set_si(ret_mpz_t, (long int)(c.bv1_.get_value()));
        break;
      }

    mpz_class ret(ret_mpz_t);
    mpz_clear(ret_mpz_t); // Free memory
    return ret;
  }

  utils::Value convert_to_value(mpz_class c, Bitsize b, bool is_signed) {
    switch(b) {
    case utils::sixty_four: {
      LONG_UINT val;
      if(is_signed)
        val = (LONG_UINT)mpz_get_sll(c);
      else
        val = mpz_get_ull(c);
      return utils::Value(b, val);
    }
    case utils::thirty_two: {
      UINT val;
      if(is_signed)
        val = (UINT)mpz_get_si(c.get_mpz_t());
      else
        val = mpz_get_ui(c.get_mpz_t());
      return utils::Value(BV32(val));
    }
    case utils::sixteen: {
      USHORT val;
      if(is_signed)
        val = (USHORT)mpz_get_si(c.get_mpz_t());
      else
        val = (USHORT)mpz_get_ui(c.get_mpz_t());
      return utils::Value(BV16(val));
    }
    case utils::eight: {
      UCHAR val;
      if(is_signed)
        val = (UCHAR)mpz_get_si(c.get_mpz_t());
      else
        val = (UCHAR)mpz_get_ui(c.get_mpz_t());
      return utils::Value(BV8(val));
    }
    case utils::one: {
      UCHAR val;
      if(is_signed)
        val = (UCHAR)mpz_get_si(c.get_mpz_t());
      else
        val = (UCHAR)mpz_get_ui(c.get_mpz_t());
      return utils::Value(BV1(val));
    }
    }
    return utils::Value(b, 0ull);
  }

  utils::Value GetMaxInt(utils::Bitsize b, bool is_signed) {
    utils::Value max;
    switch(b) {
    case utils::sixty_four:
      max = is_signed? BV64(0x7fffffffffffffffll): BV64(0xffffffffffffffffull); 
      break;
    case utils::thirty_two:
      max = is_signed? BV32(0x7fffffff): BV32(0xffffffffu); 
      break;
    case utils::sixteen:
      max = is_signed? BV16(0x7fff): BV16(0xffffu); 
      break;
    case utils::eight:
      max = is_signed? BV8(0x7f): BV8(0xffu); 
      break;
    case utils::one:
      max = BV1(1); 
      break;
    }
    return max;
  }

  utils::Value GetMinInt(utils::Bitsize b, bool is_signed) {
    utils::Value min;
    switch(b) {
    case utils::sixty_four:
      min = is_signed? BV64(0x8000000000000000ll): BV64(0x0000000000000000ull); 
      break;
    case utils::thirty_two:
      min = is_signed? BV32(0x80000000): BV32(0x00000000u); 
      break;
    case utils::sixteen:
      min = is_signed? BV16(0x8000): BV16(0x0000u); 
      break;
    case utils::eight:
      min = is_signed? BV8(0x80): BV8(0x00u); 
      break;
    case utils::one:
      min = BV1(0); 
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

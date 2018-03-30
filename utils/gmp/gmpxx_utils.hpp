#ifndef utils_gmp_gmpxx_utils_hpp
#define utils_gmp_gmpxx_utils_hpp
#include "gmpxx.h"
#endif
#include "value.hpp"

namespace utils {


  // 64-bit get and set functions for gmp mpz class 
  // picked up from http://stackoverflow.com/questions/6248723/mpz-t-to-unsigned-long-long-conversion-gmp-lib
  mpz_class mpz_set_sll(LONG_INT sll);
  mpz_class mpz_set_ull(LONG_UINT ull);
  LONG_UINT mpz_get_ull(mpz_class n);
  LONG_INT mpz_get_sll(mpz_class n);

  mpz_class convert_to_mpz(Value c, bool is_signed);
  Value convert_to_value(mpz_class c, Bitsize b, bool is_signed);

  Value GetMaxInt(Bitsize b, bool is_signed);
  Value GetMinInt(Bitsize b, bool is_signed);

  // mpz rshift and lshift utilities
  // Perform a >>a uint(b), where b is truncated to uint number
  mpz_class mpz_rshift_arith(const mpz_class a, const mpz_class b);
  mpz_class mpz_lshift(const mpz_class a, const mpz_class b);

  // Logical functions
  mpz_class mpz_and(const mpz_class a, const mpz_class b);
  mpz_class mpz_or(const mpz_class a, const mpz_class b);
  mpz_class mpz_xor(const mpz_class a, const mpz_class b);

} //namespace utils
#endif // utils_gmp_gmpxx_utils_hpp

#include "src/AbstractDomain/common/BitpreciseWrappedAbstractValue.hpp"

namespace abstract_domain {

  bool BitpreciseWrappedAbstractValue::lazy_wrapping = true;
  bool BitpreciseWrappedAbstractValue::disable_wrapping = false;

  std::pair<std::pair<BitpreciseWrappedAbstractValue::affexp_type, 
                      BitpreciseWrappedAbstractValue::affexp_type>, 
            BitpreciseWrappedAbstractValue::OpType> 
  BitpreciseWrappedAbstractValue::GetMinBoundingConstraint(const DimensionKey& k, bool is_signed) {
    mpz_class one_mpz(1);
    mpz_class zero_mpz(0);
    unsigned b = k.GetNumbits();

    linexp_type k_le; k_le.insert(linexp_type::value_type(k, one_mpz)); // Creates expr 1.k
    mpz_class min_mpz = utils::convert_to_mpz(GetMinInt(b, is_signed), is_signed);

    // 1.k >= min_mpz
    return std::make_pair(std::make_pair(affexp_type(k_le, zero_mpz), affexp_type(linexp_type(), min_mpz)), GE); 
  }

  std::pair<std::pair<BitpreciseWrappedAbstractValue::affexp_type, BitpreciseWrappedAbstractValue::affexp_type>, 
            BitpreciseWrappedAbstractValue::OpType> 
  BitpreciseWrappedAbstractValue::GetMaxBoundingConstraint(const DimensionKey& k, bool is_signed) {
    mpz_class one_mpz(1);
    mpz_class zero_mpz(0);
    utils::Bitsize b = k.GetBitsize();

    linexp_type k_le; k_le.insert(linexp_type::value_type(k, one_mpz)); // Creates expr 1.k
    mpz_class max_mpz = utils::convert_to_mpz(GetMaxInt(b, is_signed), is_signed);

    // 1.k <= max_mpz
    return std::make_pair(std::make_pair(affexp_type(k_le, zero_mpz), affexp_type(linexp_type(), max_mpz)), LE);
  }

  // Add wrapping constraints for v, cheap alternative to wrap when it is known that vocabulary
  // v is unconstrained
  void BitpreciseWrappedAbstractValue::AddBoundingConstraints(const VocabularySignedness& vs) {
    Vocabulary voc;
    for(VocabularySignedness::const_iterator it = vs.begin(); it != vs.end(); it++) {
      DimensionKey k = it->first;
      voc.insert(k);
    }
    AddVocabulary(voc);

    for(VocabularySignedness::const_iterator it = vs.begin(); it != vs.end(); it++) {
      DimensionKey k = it->first;
      bool is_signed = it->second;
      if(!disable_wrapping) {
        
        std::pair<std::pair<affexp_type, affexp_type>, OpType> min_constr = GetMinBoundingConstraint(k, is_signed); 
        std::pair<std::pair<affexp_type, affexp_type>, OpType> max_constr = GetMaxBoundingConstraint(k, is_signed); 

        AddConstraint(min_constr.first.first, min_constr.first.second, min_constr.second);
        AddConstraint(max_constr.first.first, max_constr.first.second, max_constr.second);
      }
      wrapped_voc_[k] = is_signed;
    }
  }

  void BitpreciseWrappedAbstractValue::UpdateVocabularySignedness(Vocabulary voc) {
    ref_ptr<AbstractValue> av_cp = av_->Copy();
    for(Vocabulary::const_iterator it = voc.begin(), end = voc.end(); it != end; it++) {
      DimensionKey k = *it;
      VocabularySignedness::const_iterator wv_it = wrapped_voc_.find(k);
      if(wv_it == wrapped_voc_.end()) {
        // First check if this vocabulary is bounded by signed boundary variables
        bool is_signed = true;
        bool is_signed_orig = is_signed;
        do {
          std::pair<std::pair<affexp_type, affexp_type>, OpType> min_cnstr = GetMinBoundingConstraint(k, is_signed); 
          std::pair<std::pair<affexp_type, affexp_type>, OpType> max_cnstr = GetMaxBoundingConstraint(k, is_signed); 
          av_cp->AddConstraint(min_cnstr.first.first, min_cnstr.first.second, min_cnstr.second);
          av_cp->AddConstraint(max_cnstr.first.first, max_cnstr.first.second, max_cnstr.second);

          if(*av_cp == *av_) {
            wrapped_voc_.insert(std::make_pair(k, is_signed));
            break;
          }
          av_cp = av_->Copy();
          is_signed = !is_signed;
        }
        while(is_signed != is_signed_orig);
      }
    }
  }
}

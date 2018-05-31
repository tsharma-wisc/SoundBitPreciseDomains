#include "dimension.hpp"
#include "string.h"
#include <set>
#include <algorithm>
#include <sstream>

const abstract_domain::Version abstract_domain::UNVERSIONED_VERSION = (Version)-1;
const abstract_domain::DimensionKey 
abstract_domain::DUMMY_KEY("dummy", 
                           abstract_domain::UNVERSIONED_VERSION, 
                           utils::one);

namespace abstract_domain {

  bool DimensionKey::operator<(const DimensionKey &that) const {
    if(name != that.name) {
      bool result = name < that.name;
      return result;
    }
    if(ver != that.ver) {
      bool result = ver < that.ver;
      return result;
    }

    // TODO: Handle complex sort such as arrays and bv. For now they are ignored as the expectection is
    // that sort would be a bool, int or real sort only.
    bool result = (bitsize < that.bitsize);
    return result;
  }

  bool DimensionKey::operator==(const DimensionKey &that) const {
    return (!(*this < that) && !(that < *this));
  }

  utils::Bitsize DimensionKey::GetBitsize() const {
    return bitsize;
  }

  void UnionVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res) {
    res.clear();
    std::insert_iterator<Vocabulary> res_ins(res, res.begin());
    std::set_union(first.begin(), first.end(), second.begin(), second.end(), res_ins);
  }

  void IntersectVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res) {
    res.clear();
    std::insert_iterator<Vocabulary> res_ins(res, res.begin());
    std::set_intersection(first.begin(), first.end(), second.begin(), second.end(), res_ins);
  }

  void SubtractVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res) {
    res.clear();
    std::set_difference(first.begin(), first.end(), second.begin(), second.end(), std::inserter(res, res.end()));
  }

  DimensionKey replaceVersion(const DimensionKey& k, Version from, Version to) {
    Version old_ver = k.ver;
    if(old_ver == from) {
      return DimensionKey(k.name, to, k.bitsize);
    }
    return k;
  }

  DimensionKey replaceVersions(const DimensionKey& k, const std::map<Version, Version>& map) {
    Version old_ver = k.ver;
    for(std::map<Version, Version>::const_iterator map_it = map.begin(); map_it != map.end(); map_it++) {
      DimensionKey new_k = replaceVersion(k, map_it->first, map_it->second);
      if(new_k != k) {
        return new_k;
      }
    }
    return k;
  }

  Vocabulary replaceVersion(const Vocabulary& v, Version from, Version to) {
    Vocabulary ret;
    for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
      ret.insert(replaceVersion(*it, from, to));
    }
    return ret;
  }

  Vocabulary replaceVersions(const Vocabulary& v, const std::map<Version, Version>& map) {
    Vocabulary ret;
    for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
      DimensionKey val = *it;
      for(std::map<Version, Version>::const_iterator map_it = map.begin(); map_it != map.end(); map_it++) {
        DimensionKey new_val = replaceVersion(*it, map_it->first, map_it->second);
        if(new_val != val) {
          val = new_val;
          break;
        }
        ret.insert(val);
      }
    }
    return ret;
  }

  bool isVersionInDimension(const DimensionKey& k, Version v) {
    return (k.ver == v);
  }

  //Get the subset of the vocabulary 'voc' which talks
  //about version 'v'.
  Vocabulary getVocabularySubset(const Vocabulary& voc, Version v) {
    Vocabulary ret;
    Vocabulary::const_iterator it;
    for(it = voc.begin(); it != voc.end(); it++) {
      if(isVersionInDimension(*it, v)) {
        ret.insert(*it);
      }
    }
    return ret;
  }

  Vocabulary getUnversionedVocabularySubset(const Vocabulary& voc) {
    return getVocabularySubset(voc, UNVERSIONED_VERSION);
  }

  Vocabulary getVocabularySubset(const Vocabulary& voc, utils::Bitsize bitsize) {
    Vocabulary ret;
    Vocabulary::const_iterator it;
    for(it = voc.begin(); it != voc.end(); it++) {
      if(it->bitsize == bitsize) {
        ret.insert(*it);
      }
    }
    return ret;
  }

  bool correspondingDimensions(const DimensionKey& k1, const DimensionKey& k2) {
    bool is_k1_ver0 = isVersionInDimension(k1, 0);
    if(is_k1_ver0) {
      DimensionKey k1_1 = replaceVersion(k1, 0, 1);
      if(k1_1 == k2)
        return true;
    } else {
      DimensionKey k1_0 = replaceVersion(k1, 1, 0);
      if(k1_0 == k2)
        return true;
    }
    return false;
  }

  std::ostream & print(std::ostream & out, const DimensionKey& k) {
    out << std::dec << k.name << "_" << k.ver << "_" << k.bitsize;
    return out;
  }

  std::ostream & print(std::ostream & out, const Vocabulary& v, std::string delimiter) {
    out << " (voc_size:" << v.size() << ") ";
    for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
      print(out, *it);
      out << delimiter;
    }
    return out;
  }
}

std::ostream & operator<<(std::ostream & out, const abstract_domain::DimensionKey& k) {
  return abstract_domain::print(out, k);
}

std::ostream & operator<<(std::ostream & out, const abstract_domain::Vocabulary& v) {
  return abstract_domain::print(out, v);
}

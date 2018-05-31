#ifndef src_AbstractDomain_common_dimension_hpp
#define src_AbstractDomain_common_dimension_hpp

#include <map>
#include <set>
#include "utils/gmp/gmpxx_utils.hpp"

namespace abstract_domain {
  // Version is defined as 0 for pre-vocabulary, 1 for post-vocabulary,
  // and UNVERSIONED_VERSION for unversioned vocabulary.
  // Unversioned vocabulary is used by single static assignment (ssa) variables.
  typedef unsigned Version;

  // UNVERSIONED_VERSION is defined to be Version(-1)
  extern const Version UNVERSIONED_VERSION;

  class DimensionKey {
  public:
    std::string name;
    Version ver;
    utils::Bitsize bitsize;

    DimensionKey() : name(""), ver(0u), bitsize(utils::thirty_two) {}
    DimensionKey(const std::string& _name, Version _ver, utils::Bitsize _bitsize) : name(_name), ver(_ver), bitsize(_bitsize) {}

    // Required to create sets over this class
    bool operator<(const DimensionKey &that) const;

    bool operator==(const DimensionKey &that) const;

    bool operator!=(const DimensionKey &that) const {
      return !(*this == that);
    }

    utils::Bitsize GetBitsize() const;
  };

  // Vocabulary is simply defined as a set of DimensionKey.
  typedef std::set<DimensionKey> Vocabulary;

  void UnionVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res);
  void IntersectVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res);
  void SubtractVocabularies(const Vocabulary &first, const Vocabulary &second, Vocabulary &res);

  DimensionKey replaceVersion(const DimensionKey& i, Version from, Version to);
  DimensionKey replaceVersions(const DimensionKey& v, const std::map<Version, Version>& map);
  Vocabulary replaceVersion(const Vocabulary& v, Version from, Version to);
  Vocabulary replaceVersions(const Vocabulary& v, const std::map<Version, Version>& map);

  bool isVersionInDimension(const DimensionKey&, Version);

  // Get the subset of the vocabulary 'voc' which talks
  // about version 'v'.
  Vocabulary getVocabularySubset(const Vocabulary& voc, Version v);
  Vocabulary getUnversionedVocabularySubset(const Vocabulary& voc);

  bool correspondingDimensions(const DimensionKey&, const DimensionKey&);

  std::ostream & print(std::ostream & out, const DimensionKey& k);
  std::ostream & print(std::ostream & out, const Vocabulary& v, std::string delimiter = "|");

  // Dummy key placeholder
  extern const DimensionKey DUMMY_KEY;
}

std::ostream & operator<<(std::ostream & out, const abstract_domain::DimensionKey& k);
std::ostream & operator<<(std::ostream & out, const abstract_domain::Vocabulary& v);

#endif // src_AbstractDomain_common_dimension_hpp

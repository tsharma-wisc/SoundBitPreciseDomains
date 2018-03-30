#include "AbstractValue.hpp"

namespace abstract_domain {
  // constructors
  AbstractValue::AbstractValue() {}
  AbstractValue::AbstractValue(Vocabulary v) : voc_(v) {}

  // Destructor
  AbstractValue::~AbstractValue() {}

  std::ostream& AbstractValue::print(std::ostream & out) const {
    out << ToString();
    return out;           
  }

  bool AbstractValue::Equal(const wali::ref_ptr<AbstractValue> & av) const {
    return (*this == *av);
  }

  bool AbstractValue::operator!=(const AbstractValue& av) const {
    return !(*this == av);
  }

  Vocabulary AbstractValue::GetVocabulary() const { 
    return voc_; 
  }

  size_t AbstractValue::NumVars() const { 
    return voc_.size();
  }

  // Widen operation should be overridden for domains with infinite lattices
  void AbstractValue::Widen(const wali::ref_ptr<AbstractValue> & that) { 
    Join(that); 
  }

  void AbstractValue::AddDimension(const DimensionKey & k) {
    Vocabulary voc;
    voc.insert(k);
    AddVocabulary(voc);
  }

  void AbstractValue::RemoveVocabulary (const Vocabulary & voc) {
    //Get proj_voc (Vocabulary to project on) from voc
    Vocabulary proj_voc;

    for(Vocabulary::const_iterator it = this->voc_.begin(); it != this->voc_.end(); it++) {
      if(voc.find(*it) == voc.end())
        proj_voc.insert(*it);
    }
    Project(proj_voc);
  }

  void AbstractValue::RemoveDimension(const DimensionKey & k) {
    Vocabulary voc;
    voc.insert(k);
    RemoveVocabulary(voc);
  }

  void AbstractValue::AddVersion(const Version& old_ver, const Version& new_ver) {
    Vocabulary v;
    for(Vocabulary::const_iterator it = this->voc_.begin(); it != this->voc_.end(); it++) {
      v.insert(replaceVersion(*it, old_ver, new_ver));
    }
    AddVocabulary(v);
  }

  void AbstractValue::ReplaceVersion(const Version & o, const Version & n) {
    std::map<Version, Version> map;
    map.insert(std::pair<Version, Version>(o, n));
    ReplaceVersions(map);
  }

  // Each AbstractValue can implement its own extend to achieve more performance or precision
  bool AbstractValue::ImplementsExtend() {
    return false;
  }

  // Default implementation of extend throws an assertion
  wali::ref_ptr<AbstractValue> AbstractValue::Extend(const wali::ref_ptr<AbstractValue> &that) const { 
    std::cout << "The base class AbstractValue does not define Extend.";
    assert(false);
    return Copy();
  }
}

std::ostream & operator <<(std::ostream & out, const abstract_domain::AbstractValue& a) {
  return a.print(out);
}

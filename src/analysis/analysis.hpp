#ifndef src_analysis_analysis_hpp
#define src_analysis_analysis_hpp

int analysis();

extern const char* cmdlineparam_filename;
extern unsigned cmdlineparam_max_disjunctions;
extern bool cmdlineparam_disable_wrapping;
extern bool cmdlineparam_use_oct;
extern bool cmdlineparam_use_red_prod;
extern bool cmdlineparam_use_fwpds;
extern bool cmdlineparam_use_extrapolation;
extern bool cmdlineparam_perform_narrowing;
extern bool cmdlineparam_array_bounds_check;
extern bool cmdlineparam_allow_phis;

#endif // src_analysis_analysis_hpp

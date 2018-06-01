#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "analysis.hpp"

bool cmdlineparam_use_oct = false;
bool cmdlineparam_use_red_prod = false;
bool cmdlineparam_use_fwpds = false;

// This flag specifies whether to disable wrapping while performing analysis.
// This can lead to unsound result.
bool cmdlineparam_disable_wrapping = false;

// This flag specifies whether to use extrapolation operator instead of widening
// The advantage is that it can take number of maximum allowed disjunctions as parameter,
// and thus extrapolation is more
bool cmdlineparam_use_extrapolation = false;

// This flag specifies whether to perform narrowing (ie downward iteration sequence) 
// before querying of the unreachable points. 
// This can provide more precision at the expense of some efficiency.
bool cmdlineparam_perform_narrowing = false;

// Cmdline parameter specifying whether to check if the assertions added as array bounds check
// are unreachable
bool cmdlineparam_array_bounds_check = false;

// Cmdline parameter specifying whether to change the analysis to expect phis
bool cmdlineparam_allow_phis = false;

unsigned cmdlineparam_max_disjunctions = 1;

std::string cmdlineparam_filename;

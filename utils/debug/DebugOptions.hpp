#ifndef utils_debug_debugoptions_hpp
#define utils_debug_debugoptions_hpp

extern unsigned int debug_print_level;
extern unsigned int debug_assert_level;
extern bool enable_formula_printing;

/***** Define the debug_print_level levels ******/
// Print progress of analysis
#define DBG_PRINT_OVERVIEW 1
// Print operations at debugging level 2 or greater
#define DBG_PRINT_OPERATIONS 2
// Print micro-level details at debugging level 3 or greater
#define DBG_PRINT_DETAILS 3
// Print more detailed information
#define DBG_PRINT_MORE_DETAILS 4
// Print every conceivable information for debugging
#define DBG_PRINT_EVERYTHING 5

/***** Define the debug_assert_level levels ******/
// Check lightweight assertions
#define LIGHTWEIGHT_ASSERTIONS 1
// Check heavyweight assertions
#define HEAVYWEIGHT_ASSERTIONS 2

#ifdef NDEBUG
#define DEBUG_PRINTING(l,x) do {} while(false)
#else
#define DEBUG_PRINTING(l,x) if(debug_print_level >= l) {x}
#endif

#endif // utils_debug_debugoptions_hpp

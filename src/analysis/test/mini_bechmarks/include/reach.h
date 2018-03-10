#ifndef REACH_H
#define REACH_H

/* If either of these two magic numbers change, the Python
   testing script should change too                          */
#define UNREACHABLE_MAGIC 0x2A00
#define REACHABLE_MAGIC   0x2A10

// These lines need to be kept in sync with regression/SConstruct at
// get_makechoice_wrap
#define MAKECHOICE_MAGIC  0x2A20
#define FOPEN_MAGIC   0x2A21
#define FREAD_MAGIC   0x2A22
#define FWRITE_MAGIC  0x2A23
#define FCLOSE_MAGIC  0x2A24
#define FSEEK_MAGIC   0x2A25
#define FTELL_MAGIC   0x2A26

#define STRINGIZE(obj) #obj

#if defined(__GNUC__)
#  if defined(__i386__)
#    define INCLUDE_ASM(magic)  __asm__("cmpl $" STRINGIZE(magic) ", %%eax" : : : "cc")
#  elif defined(__PPC)
#    define INCLUDE_ASM(magic)  __asm__("cmpwi cr7,0," STRINGIZE(magic))
#  else
#    define INCLUDE_ASM(magic)  __asm__("cmpl $" STRINGIZE(magic) ", %%eax" : : : "cc")
#  endif
#endif

#if defined(_MSC_VER)
#  define INCLUDE_ASM(magic)  __asm cmp eax, magic
#endif



#if defined(_MSC_VER)
typedef unsigned long my_size_t;
#else
typedef __SIZE_TYPE__ my_size_t;
#endif



#define REACHABLE() 	INCLUDE_ASM(REACHABLE_MAGIC);
#define UNREACHABLE() 	INCLUDE_ASM(UNREACHABLE_MAGIC);

void reachable();
void unreachable();

// canary for buffer overflow detection (mostly used in verisec tests)
#define BRIGHT_YELLOW 23

// These definitions are needed when tool='semanalysis'. The tool works over all the functions and hence prefers to
// work over 'obj' instead of 'exe'. These definitions are needed so that 'obj' listing file has the MAGIC code 
// in it. The MAGIC codes are needed to get appropriate targets.
#ifdef INLINE_SPECIAL_FUNCS

#endif // INLINE_SPECIAL_FUNCS


#endif // REACH_H


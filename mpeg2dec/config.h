/* config.h, configuration defines                                          */
/* define NON_ANSI_COMPILER for compilers without function prototyping */
/* #define NON_ANSI_COMPILER */

#ifdef NON_ANSI_COMPILER
  #define _ANSI_ARGS_(x) ()
#else
  #define _ANSI_ARGS_(x) x
#endif

#define RB "rb"
#define WB "wb"

#ifndef O_BINARY
  #define O_BINARY 0
#endif

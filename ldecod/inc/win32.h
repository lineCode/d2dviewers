#pragma once

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define NUM_THREADS 8

#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <crtdefs.h>

#define  strcasecmp _strcmpi
#define  strncasecmp _strnicmp
#define  snprintf _snprintf
#define  open     _open
#define  close    _close
#define  read     _read
#define  write    _write
#define  lseek    _lseeki64
#define  fsync    _commit
#define  tell     _telli64
#define  TIMEB    _timeb
#define  TIME_T    LARGE_INTEGER
#define  OPENFLAGS_WRITE _O_WRONLY|_O_CREAT|_O_BINARY|_O_TRUNC
#define  OPEN_PERMISSIONS _S_IREAD | _S_IWRITE
#define  OPENFLAGS_READ  _O_RDONLY|_O_BINARY
#define  inline   _inline
#define  forceinline __forceinline

typedef __int64   int64;
typedef unsigned __int64   uint64;
#define FORMAT_OFF_T "I64d"
#ifndef INT64_MIN
  #define INT64_MIN        (-9223372036854775807i64 - 1i64)
#endif

extern void gettime (TIME_T* time);
extern void init_time (void);
extern int64 timediff (TIME_T* start, TIME_T* end);
extern int64 timenorm (int64 cur_time);

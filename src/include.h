#ifndef __INCLUDE_H__
#define __INCLUDE_H__

#include <stdint.h>
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint8_t uint8;
typedef int8_t int8;
typedef int16_t int16;
typedef uint16_t uint16;

#ifdef WIN32
#define PACKED_ON(name) __pragma(pack(push, 1)) struct name
#define PACKED_OFF __pragma(pack(pop))
#else
#define PACKED_ON(name) struct __attribute__((__packed__)) name
#define PACKED_OFF
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef WIN32
#include "getopt.h"
#ifndef NO_IO
#include <io.h>
#endif
#include <windows.h>
#else
#include <getopt.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include "printf.h"
#define S_IFMT 170000
#define S_IFDIR 40000
#define S_IFBLK 60000
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#define usleep(n) Sleep(n / 1000)
#endif
#include "ptable.h"
#include "qcio.h"
#endif
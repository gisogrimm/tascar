#ifndef _WINDOWS_PORT_H
#define _WINDOWS_PORT_H

#ifdef WIN32 
#include <algorithm> // we need this header for std::min and std::max
#include <limits> 
#pragma warning(disable : 4244) // Code is full of double to float
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#include <iomanip>
#include <sstream>
#include <time.h>

extern "C" char* strptime(const char* s, const char* f, struct tm* tm);
extern "C" void usleep(__int64 usec);
extern "C" bool fnmatch_win32(const char* mask, const char* name);

#define popen _popen

#endif _WINDOWS_PORT_H

#ifdef WIN32
// Some nasty Windows header declare MIN and MAX macros, remove them every time this header is included!
#undef min
#undef max
#endif
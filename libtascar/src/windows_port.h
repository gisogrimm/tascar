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

#endif _WINDOWS_PORT_H

#ifdef WIN32
// Some nasty Windows header declare MIN and MAX macros, remove them  everytime this header is included!
#undef min
#undef max
#endif
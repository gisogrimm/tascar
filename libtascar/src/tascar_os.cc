/* License (GPL)
 *
 * Copyright (C) 2021 Hörzentrum Oldenburg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include "tascar_os.h"
#include <iomanip>
#include <sstream>
#include <string.h>
#ifdef _WIN32
#include <algorithm>
#include <shlwapi.h>
#include <windows.h>
#else
#include <fnmatch.h>
#endif
#include "tscconfig.h"
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>

namespace TASCAR {
  const char* strptime(const char* s, const char* f, struct tm* tm)
  {
#ifdef _WIN32
    std::istringstream input(s);
    input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
    input >> std::get_time(tm, f);
    if(input.fail()) {
      return nullptr;
    }
    return s + input.tellg();
#else
    return ::strptime(s, f, tm);
#endif
  }

  const char* dynamic_lib_extension(void)
  {
#if defined(__APPLE__)
    return ".dylib";
#elif __linux__
    return ".so";
#elif _WIN32
    return ".dll";
#else
#error unsupported target platform
#endif
  }

  int fnmatch(const char* pattern, const char* string, bool fnm_pathname)
  {
#ifdef _WIN32
    size_t len_p = strlen(pattern), len_s = strlen(string);
    if(len_p == 0U && len_s > 0U) {
      // PathMatchSpecA would treat this as a match, therefore intercept here.
      return -1;
    }
    if(fnm_pathname && std::count(pattern, pattern + len_p, '/') !=
                           std::count(string, string + len_s, '/')) {
      // The check for same number of literal forward slashes may
      // or may not be sufficient to mimick fnmatch behaviour in
      // FNM_PATHNAME mode when combined with PathMatchSpecA.
      // We will not search for a mathematical proof, this should
      // be sufficient for TASCAR use cases.
      return -1;
    }
    return not PathMatchSpecA(string, pattern);
#else
    return ::fnmatch(pattern, string, fnm_pathname ? FNM_PATHNAME : 0);
#endif
  }

  const char* realpath(const char* path, char* resolved_path)
  {
#ifdef _WIN32
    DWORD err = GetFullPathNameA(path, MAX_PATH, resolved_path, nullptr);
    if(err == 0 || err > MAX_PATH) {
      return path;
    }
    return resolved_path;
#else
    const char* result = ::realpath(path, resolved_path);
    if(result == nullptr) {
      return path;
    }
    return result;
#endif
  }

#ifdef _WIN32

  std::map<pid_t, PROCESS_INFORMATION> pidmap;

#endif

  pid_t system(const char* command, bool shell, bool win_showwindow)
  {
    pid_t pid = -1;
#ifndef _WIN32 // Windows has no fork.
    if(win_showwindow) {
    };
    pid = fork();
    if(pid < 0) {
      return pid;
    } else if(pid == 0) {
      /// Close all other descriptors for the safety sake.
      for(int i = 3; i < 4096; ++i)
        ::close(i);
      setsid();
      if(!shell) {
        std::vector<std::string> pars = TASCAR::str2vecstr(command);
        char* vpars[pars.size() + 1];
        for(size_t k = 0; k < pars.size(); ++k) {
          vpars[k] = strdup(pars[k].c_str());
        }
        vpars[pars.size()] = NULL;
        if(pars.size()) {
          execvp(pars[0].c_str(), vpars);
        }
        for(size_t k = 0; k < pars.size(); ++k) {
          free(vpars[k]);
        }
      } else {
        execl("/bin/sh", "sh", "-c", command, NULL);
      }
      _exit(1);
    }
#else
    // windows:
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;
    std::string cmd = command;
    DWORD dwCreationFlags = 0;
    if(!win_showwindow)
      dwCreationFlags |= CREATE_NO_WINDOW;
    if(shell)
      cmd = "cmd /c " + cmd;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));
    if(CreateProcess(NULL, const_cast<char*>(command), NULL, NULL, FALSE,
                     dwCreationFlags, NULL, NULL, &startup_info,
                     &process_info)) {
      pid = 1;
      bool found = false;
      do {
        found = false;
        for(const auto& pide : pidmap)
          if(pide.first == pid)
            found = true;
        if(found)
          ++pid;
      } while(found);
      pidmap[pid] = process_info;
    } else {
      DEBUG("Pailed to start process");
      DEBUG(command);
    }
#endif
    return pid;
  }

  void terminate_process(pid_t pid)
  {
#ifndef _WIN32
    if(pid != 0) {
      killpg(pid, SIGTERM);
    }
#else
    if(auto pide = pidmap.find(pid) != pidmap.end()) {
      TerminateProcess(pidmap[pid].hProcess, 0);
      CloseHandle(pidmap[pid].hProcess);
      CloseHandle(pidmap[pid].hThread);
      pidmap.erase(pid);
    } else {
      DEBUG("Unable to stop process " + std::to_string(pid) +
            ": no handle found");
      DEBUG(pidmap.size());
      for(auto lpid : pidmap) {
        DEBUG(lpid.first);
        DEBUG(lpid.second.dwProcessId);
      }
    }
#endif
  }

#ifdef _WIN32
  void wait_for_process(pid_t pid)
  {
    if(pidmap.find(pid) != pidmap.end()) {
      WaitForSingleObject(pidmap[pid].hProcess, INFINITE);
    }
  }
#endif

  void* dlopen(const char* filename, int flags)
  {
    // First try the normal dynamic linker search path (RPATH,
    // LD_LIBRARY_PATH, ld.so cache).
    auto lib = ::dlopen(filename, flags);
    // Fall-back to the configured TASCAR plugin directory (baked in
    // via TASCAR_DEFAULT_PLUGINDIR or set at runtime via set_libdir).
    if(!lib) {
      const auto& libdir = TASCAR::get_libdir();
      if(!libdir.empty())
        lib = ::dlopen((libdir + filename).c_str(), flags);
    }
    if(!lib) {
      auto homebrewprefix = localgetenv("HOMEBREW_PREFIX");
      if(homebrewprefix.size()) {
        homebrewprefix += "/lib/";
        homebrewprefix += filename;
        lib = ::dlopen(homebrewprefix.c_str(), flags);
      }
    }
    return lib;
  }

} // namespace TASCAR

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

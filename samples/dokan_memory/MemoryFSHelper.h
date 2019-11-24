#pragma once

#include <windows.h>

class MemoryFSHelper {
 public:
  static inline LONGLONG FileTimeToLlong(const FILETIME& f) {
    return DDwLowHighToLlong(f.dwLowDateTime, f.dwHighDateTime);
  }

  static inline void LlongToFileTime(LONGLONG v, FILETIME& fileTime) {
    LlongToDwLowHigh(v, fileTime.dwLowDateTime, fileTime.dwHighDateTime);
  }

  static inline LONGLONG DDwLowHighToLlong(const DWORD& low, const DWORD& high) {
    return static_cast<LONGLONG>(high) << 32 | low;
  }

  static inline void LlongToDwLowHigh(const LONGLONG& v, DWORD& low,
                                   DWORD& hight) {
    hight = v >> 32;
    low = static_cast<DWORD>(v);
  }
};
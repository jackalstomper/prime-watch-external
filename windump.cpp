#include "common.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tlhelp32.h>

#include <iostream>
#include <cstring>

using namespace std;

void wootWootGetRoot() {
  HANDLE handle;
  LUID luid;
  TOKEN_PRIVILEGES privileges;

  OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &handle);

  LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);

  privileges.PrivilegeCount = 1;
  privileges.Privileges[0].Luid = luid;
  privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  AdjustTokenPrivileges(handle, false, &privileges, sizeof(privileges), NULL, NULL);

  CloseHandle(handle);
}

HANDLE dolphinProcHandle = 0;
void *offset = (void *) 0x7FFF0000;

void findAndAttachProcess() {
  PROCESSENTRY32 entry;
  entry.dwSize = sizeof(PROCESSENTRY32);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  bool found = false;
  if (Process32First(snapshot, &entry) == TRUE) {
    while (Process32Next(snapshot, &entry) == TRUE) {
      if (strncmp(entry.szExeFile, "Dolphin.exe", 10) == 0) {
        cout << "Found Dolphin, PID " << entry.th32ProcessID << endl;
        dolphinProcHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
        found = true;
        break;
      }
    }
  }

  CloseHandle(snapshot);

  if (!found) {
    cerr << "Can't find dolphin. Launch dolphin first!" << endl;
    exit(1);
  }
}

namespace GameMemory {
    uint32_t getRealPtr(std::uint32_t address) {
      uint32_t masked = address & 0x7FFFFFFF;
      return masked;
    }

    uint32_t read_u32(uint32_t address) {
      uint32_t res = 0;
      SIZE_T read = 0;
      ReadProcessMemory(dolphinProcHandle, offset + getRealPtr(address), &res, 4, &read);
      if (read != 4) {
        return 0;
      } else {
        return be32toh(res);
      }
    }

    uint64_t read_u64(uint32_t address) {
      uint64_t res = 0;
      SIZE_T read = 0;
      ReadProcessMemory(dolphinProcHandle, offset + getRealPtr(address), &res, 8, &read);
      if (read != 8) {
        return 0;
      } else {
        return be64toh(res);
      }
    }

    float read_float(uint32_t address) {
      union {
          uint32_t i;
          float f;
      } u;
      u.i = read_u32(address);
      return u.f;
    }

    double read_double(uint32_t address) {
      union {
          uint64_t i;
          double d;
      } u;
      u.i = read_u64(address);
      return u.d;
    }
}
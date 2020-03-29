#include "common.h"

#include <windows.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <iostream>
#include <cstring>

using namespace std;

HANDLE dolphinProcHandle = 0;
uint64_t emuRAMAddressStart = 0;
bool MEM2Present = false;

void ErrorExit(LPTSTR lpszFunction) {
  // Retrieve the system error message for the last-error code

  LPVOID lpMsgBuf;
  DWORD dw = GetLastError();

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dw,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR) &lpMsgBuf,
    0, NULL);

  cerr << lpszFunction << " failed with error " << dw << ": " << (LPTSTR)lpMsgBuf << endl;
  LocalFree(lpMsgBuf);
  ExitProcess(dw);
}

// Shamelessly ripped from Dolphin-memory-engine
// https://github.com/aldelaro5/Dolphin-memory-engine/blob/master/Source/DolphinProcess/Windows/WindowsDolphinProcess.cpp
// Returns true if we found and set emuRAMAddressStart
bool getEmuRAMAddressStart() {
  if (!dolphinProcHandle)
    return false;

  MEMORY_BASIC_INFORMATION info;
  bool MEM1Found = false;
  for (unsigned char* p = nullptr; VirtualQueryEx(dolphinProcHandle, p, &info, sizeof(info)) == sizeof(info); p += info.RegionSize) {
    if (MEM1Found) {
      uint64_t regionBaseAddress = 0;
      memcpy(&regionBaseAddress, &(info.BaseAddress), sizeof(info.BaseAddress));
      if (regionBaseAddress == emuRAMAddressStart + 0x10000000) {
        // View the comment for MEM1
        PSAPI_WORKING_SET_EX_INFORMATION wsInfo;
        wsInfo.VirtualAddress = info.BaseAddress;
        if (QueryWorkingSetEx(dolphinProcHandle, &wsInfo, sizeof(PSAPI_WORKING_SET_EX_INFORMATION))) {
          if (wsInfo.VirtualAttributes.Valid)
            MEM2Present = true;
        }

        break;
      } else if (regionBaseAddress > emuRAMAddressStart + 0x10000000) {
        MEM2Present = false;
        break;
      }

      continue;
    }

    if (info.RegionSize == 0x2000000 && info.Type == MEM_MAPPED) {
      // Here, it's likely the right page, but it can happen that multiple pages with these criteria
      // exists and have nothing to do with the emulated memory. Only the right page has valid
      // working set information so an additional check is required that it is backed by physical memory.
      PSAPI_WORKING_SET_EX_INFORMATION wsInfo;
      wsInfo.VirtualAddress = info.BaseAddress;
      if (QueryWorkingSetEx(dolphinProcHandle, &wsInfo, sizeof(PSAPI_WORKING_SET_EX_INFORMATION))) {
        if (wsInfo.VirtualAttributes.Valid) {
          memcpy(&emuRAMAddressStart, &(info.BaseAddress), sizeof(info.BaseAddress));
          MEM1Found = true;
          cout << "Found ram start: " << emuRAMAddressStart << endl;
        }
      }
    }
  }

  if (!emuRAMAddressStart)
    return false; // Dolphin is running, but the emulation hasn't started

  return true;
}

void findAndAttachProcess() {
  PROCESSENTRY32 entry;
  entry.dwSize = sizeof(PROCESSENTRY32);

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  bool found = false;
  if (Process32First(snapshot, &entry) == TRUE) {
    while (Process32Next(snapshot, &entry) == TRUE) {
      if (strncmp(entry.szExeFile, "Dolphin.exe", 10) == 0) {
        cout << "Found Dolphin, PID " << entry.th32ProcessID << endl;
        dolphinProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        if (dolphinProcHandle == NULL)
          ErrorExit(TEXT("OpenProcess"));

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

  if (!getEmuRAMAddressStart()) {
    // Wait for dolphin to start running a game
    cout << "Detected dolphin isn't running a game. Waiting for game to start" << endl;
    while (!getEmuRAMAddressStart())
      Sleep(1000);
  }
}

namespace GameMemory {
    uint32_t getRealPtr(uint32_t address) {
      uint32_t masked = address & 0x7FFFFFFF;
      if (masked > 0x1800000) {
        return 0;
      }
      return masked;
    }

    uint32_t read_u32(uint32_t address) {
      if (!emuRAMAddressStart)
        return 0;

      uint32_t res = 0;
      SIZE_T read = 0;
      if (ReadProcessMemory(dolphinProcHandle, reinterpret_cast<void*>(emuRAMAddressStart + getRealPtr(address)), &res, 4, &read) == 0) {
        cerr << "Failed to read memory from" << address << ". Error: " << GetLastError() << endl;
        return 0;
      }

      if (read != 4) {
        return 0;
      } else {
        return beToHost32(res);
      }
    }

    uint64_t read_u64(uint32_t address) {
      if (!emuRAMAddressStart)
        return 0;

      uint64_t res = 0;
      SIZE_T read = 0;
      if (ReadProcessMemory(dolphinProcHandle, reinterpret_cast<void*>(emuRAMAddressStart + getRealPtr(address)), &res, 8, &read) == 0) {
        cerr << "Failed to read memory from" << address << ". Error: " << GetLastError() << endl;
        return 0;
      }

      if (read != 8) {
        return 0;
      } else {
        return beToHost64(res);
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
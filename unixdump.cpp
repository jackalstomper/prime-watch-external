#include <cstdlib>
#include <iostream>
#include <cstdio>
#include "common.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

uint8_t* emuRAMAddressStart = 0;

void findAndAttachProcess() {
  int pid = dolphinPid();
  if (pid < 0) {
    cout << "Can't find dolphin! Launch dolphin first!" << endl;
    exit(1);
  }

  cout << "Dolphin found, PID " << pid << endl;
  const string file_name = "/dolphin-emu." + to_string(pid);
  int fd = shm_open(file_name.c_str(), O_RDWR, 0600);

  if (fd < 0) {
    cout << "Failed to open shared memory" << endl;
    exit(2);
  }

  cout << "Opened shmem" << endl;

  size_t size = 0x2040000;
  void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    cout << "failed to map shared memory" << endl;
    close(fd);
    exit(3);
  }

  emuRAMAddressStart = reinterpret_cast<uint8_t *>(mem);

  close(fd);

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
    if (!emuRAMAddressStart) return 0;

    uint32_t res = *reinterpret_cast<uint32_t *>(emuRAMAddressStart + getRealPtr(address));
    return beToHost32(res);
  }

  uint64_t read_u64(uint32_t address) {
    if (!emuRAMAddressStart)return 0;

    uint64_t res = *reinterpret_cast<uint64_t *>(emuRAMAddressStart + getRealPtr(address));
    return beToHost64(res);
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
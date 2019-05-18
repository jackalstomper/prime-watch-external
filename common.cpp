#include "common.h"

#ifdef WIN32
#include <intrin.h>
#include <cstdint>

uint32_t beToHost32(uint32_t bigEndian) {
    return _byteswap_ulong(bigEndian);
}

uint32_t hostToBe32(uint32_t value) {
    return _byteswap_ulong(value);
}

uint64_t beToHost64(uint64_t bigEndian) {
    return _byteswap_uint64(bigEndian);
}

uint64_t hostToBe64(uint64_t value) {
    return _byteswap_uint64(value);
}

#elif __APPLE__

#include <cstdint>
#include <i386/endian.h>

uint32_t beToHost32(uint32_t bigEndian) {
  return ntohl(bigEndian);
}

uint32_t hostToBe32(uint32_t value) {
  return htonl(value);
}

uint64_t beToHost64(uint64_t bigEndian) {
  return ntohll(bigEndian);
}

uint64_t hostToBe64(uint64_t value) {
  return htonll(value);
}

#else

#include <cstdint>

uint32_t beToHost32(uint32_t bigEndian) {
    return be32toh(bigEndian);
}

uint32_t hostToBe32(uint32_t value) {
    return htobe32(value);
}

uint64_t beToHost64(uint64_t bigEndian) {
    return be64toh(bigEndian);
}

uint64_t hostToBe64(uint64_t value) {
    return htobe64(value);
}

#endif
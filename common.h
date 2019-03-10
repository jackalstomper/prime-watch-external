#ifndef COMMON_H
#define COMMON_H

#ifdef WIN32
#   include <cstdint>
#endif

void findAndAttachProcess();

uint32_t beToHost32(uint32_t bigEndian);
uint32_t hostToBe32(uint32_t value);
uint64_t beToHost64(uint64_t bigEndian);
uint64_t hostToBe64(uint64_t value);

#endif
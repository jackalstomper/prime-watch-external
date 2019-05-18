#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void findAndAttachProcess();

int dolphinPid();

uint32_t beToHost32(uint32_t bigEndian);
uint32_t hostToBe32(uint32_t value);
uint64_t beToHost64(uint64_t bigEndian);
uint64_t hostToBe64(uint64_t value);

#ifdef __cplusplus
};
#endif


#endif
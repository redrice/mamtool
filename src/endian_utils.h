#ifndef _ENDIAN_UTILS_H_
#define _ENDIAN_UTILS_H_

#include <stdint.h>

uint16_t be16_to_host(uint16_t v);
uint32_t be32_to_host(uint32_t v);
uint64_t be64_to_host(uint64_t v);

#endif /* _ENDIAN_UTILS_H_ */


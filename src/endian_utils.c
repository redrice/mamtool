#ifdef __linux__
#include <byteswap.h>
#endif

#include "endian_utils.h"

uint16_t
be16_to_host(uint16_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#ifdef __linux__
	return bswap_16(v);
#else   
	return bswap16(v);
#endif
else
	return v;
}

uint32_t
be32_to_host(uint32_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#ifdef __linux__
	return bswap_32(v);
#else   
	return bswap32(v);
#endif
else
	return v;
}

uint64_t
be64_to_host(uint64_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#ifdef __linux__
	return bswap_64(v);
#else   
	return bswap64(v);
#endif
else
	return v;
}



/* CRC-32 (see nlcrc.h). */
#include "nlcrc.h"

#define POLY 0xEDB88320UL

nl_u32 nl_crc32_begin(void) { return 0xFFFFFFFFUL; }

nl_u32 nl_crc32_add(nl_u32 crc, const void *data, long len)
{
    const nl_u8 *p = (const nl_u8 *)data;
    long i; int k;
    for (i = 0; i < len; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++)
            crc = (crc & 1) ? (crc >> 1) ^ POLY : (crc >> 1);
    }
    return crc & 0xFFFFFFFFUL;
}

nl_u32 nl_crc32_end(nl_u32 crc) { return crc ^ 0xFFFFFFFFUL; }

nl_u32 nl_crc32(const void *data, long len)
{
    return nl_crc32_end(nl_crc32_add(nl_crc32_begin(), data, len));
}

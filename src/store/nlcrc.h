/* CRC-32 (the common reflected form, polynomial 0xEDB88320).
 *
 * Two uses: a content fingerprint for sync's three-way compare, and the
 * checksum the store keeps for each file (programme P32).
 *
 * Portable C, no allocation, no table (computed bitwise, which is small
 * and fast enough for files of this size on a Plus).
 */
#ifndef NL_CRC_H
#define NL_CRC_H

#ifndef NL_BASIC_TYPES
#define NL_BASIC_TYPES
typedef unsigned char  nl_u8;
typedef unsigned short nl_u16;
typedef unsigned long  nl_u32;
#endif

/* CRC-32 of a block. */
nl_u32 nl_crc32(const void *data, long len);

/* The same, in pieces: start with nl_crc32_begin(), add with
 * nl_crc32_add(), finish with nl_crc32_end(). */
nl_u32 nl_crc32_begin(void);
nl_u32 nl_crc32_add(nl_u32 crc, const void *data, long len);
nl_u32 nl_crc32_end(nl_u32 crc);

#endif

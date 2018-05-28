#ifdef __GNUC__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _byteswap_ulong __builtin_bswap32
#define _byteswap_uint64 __builtin_bswap64
#endif

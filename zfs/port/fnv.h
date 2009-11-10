
#ifndef fnv_h_43045174d6b7453c930acc7afc6f7abc
#define fnv_h_43045174d6b7453c930acc7afc6f7abc

//#define FNV_INIT        0x811c9dc5L
//#define FNV_MULT        0x01000193L

//#define FNVINIT(h)      (h = FNV_INIT)
//#define FNVPART(h,c)    (h = h * FNV_MULT ^ (c))

#define FNV_INIT64      0xcbf29ce484222325LL
#define FNV_MULT64      0x00000100000001b3LL


#define FNVPART64(h,c)  (h = h * FNV_MULT64 ^ (c))

inline __uint64_t fnvhash64(const char* str, int len) {
    __uint64_t h = FNV_INIT64;

    register int i = 0;
    while (i < len)
        FNVPART64(h, ((unsigned char*)str)[i++]);
}

#undef FNV_MULT64
#undef FNV_INIT64

#endif // fnv_h_43045174d6b7453c930acc7afc6f7abc

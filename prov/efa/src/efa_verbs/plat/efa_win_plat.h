#ifndef __EFA_WIN_PLAT_H__
#define __EFA_WIN_PLAT_H__

#define PATH_MAX _MAX_PATH
#define NAME_MAX _MAX_FNAME

/* This is the expected value returned from the device when querying its attributes.
 * Defining it here, as we need the value at compile time for sizing the data path
 * descriptor array appropriately.
 */
#define EFA_DEV_ATTR_MAX_SQ_SGE 2

// Redefine Linux-specific types that predate standard types
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t __s32;
typedef uint8_t __u8;

// Redefine linux-specific types that connote Big Endian data.
// Used by static analysis tools to ensure endianness is preserved.
// No such tools exist with MSVC, but defining types so Windows and
// Linux can share header files.
typedef uint64_t __be64;
typedef uint32_t __be32;
typedef uint16_t __be16;

// Declare aligned type with MSVC-specific attributes.
typedef DECLSPEC_ALIGN(64) uint64_t __aligned_u64;

typedef unsigned int uid_t;

#define rmb() MemoryBarrier()
#define wmb() MemoryBarrier()

#define PAGE_SIZE 4096

#endif /* __EFA_WIN_PLAT_H__ */
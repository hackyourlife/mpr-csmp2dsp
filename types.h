#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;

typedef float		f32;
typedef double		f64;

typedef volatile u8	vu8;
typedef volatile u16	vu16;
typedef volatile u32	vu32;
typedef volatile u64	vu64;

typedef volatile s8	vs8;
typedef volatile s16	vs16;
typedef volatile s32	vs32;
typedef volatile s64	vs64;

typedef volatile f32	vf32;
typedef volatile f64	vf64;

typedef int		BOOL;

#define	TRUE		1
#define	FALSE		0

#ifndef true
typedef int		bool;

#define	true		1
#define	false		0
#endif

#ifndef NULL
#define	NULL		((void*) 0)
#endif

#ifndef _BV
#define _BV(x)		(1L << (x))
#endif

#ifdef _MSC_VER
#define	ATTRIBUTE_ALGIN(x)  __declspec(align(x))
#define ATTRIBUTE_PACKED __pragma(pack(1))
#else
#define	ATTRIBUTE_ALGIN(x)	__attribute__((aligned(x)))
#define ATTRIBUTE_PACKED __attribute__((packed))
#endif

#ifdef _MSC_VER
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap64 _byteswap_uint64
#define __ORDER_LITTLE_ENDIAN__ 1
#define __ORDER_BIG_ENDIAN__ 2
#define __ORDER_PDP_ENDIAN__ 3

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
typedef SSIZE_T ssize_t;

/* It is very unlikely that anyone in their right mind would actually
   try to compile this program for WinNT/PowerPC though, but still... */
#ifdef _M_PPC
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
#else
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define U16B(x)		(x)
#define U32B(x)		(x)
#define U64B(x)		(x)
#define U16L(x)		__builtin_bswap16(x)
#define U32L(x)		__builtin_bswap32(x)
#define U64L(x)		__builtin_bswap64(x)
#else
#define U16B(x)		__builtin_bswap16(x)
#define U32B(x)		__builtin_bswap32(x)
#define U64B(x)		__builtin_bswap64(x)
#define U16L(x)		(x)
#define U32L(x)		(x)
#define U64L(x)		(x)
#endif

#define	asm		__asm__

#endif

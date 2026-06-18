#ifndef MISC_H
#define MISC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef glue
#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#define stringify(s)	tostring(s)
#define tostring(s)	#s
#endif

#if __has_attribute(flatten) || !defined(__clang__)
# define QEMU_FLATTEN __attribute__((flatten))
#else
# define QEMU_FLATTEN
#endif

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)
#endif

#define HOST_LONG_BITS  64



/**
 * extract32:
 * @value: the value to extract the bit field from
 * @start: the lowest bit in the bit field (numbered from 0)
 * @length: the length of the bit field
 *
 * Extract from the 32 bit input @value the bit field specified by the
 * @start and @length parameters, and return it. The bit field must
 * lie entirely within the 32 bit word. It is valid to request that
 * all 32 bits are returned (ie @length 32 and @start 0).
 *
 * Returns: the value of the bit field extracted from the input value.
 */
static inline uint32_t extract32(uint32_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 32 - start);
    return (value >> start) & (~0U >> (32 - length));
}

/**
 * extract8:
 * @value: the value to extract the bit field from
 * @start: the lowest bit in the bit field (numbered from 0)
 * @length: the length of the bit field
 *
 * Extract from the 8 bit input @value the bit field specified by the
 * @start and @length parameters, and return it. The bit field must
 * lie entirely within the 8 bit word. It is valid to request that
 * all 8 bits are returned (ie @length 8 and @start 0).
 *
 * Returns: the value of the bit field extracted from the input value.
 */
static inline uint8_t extract8(uint8_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 8 - start);
    return extract32(value, start, length);
}

/**
 * extract16:
 * @value: the value to extract the bit field from
 * @start: the lowest bit in the bit field (numbered from 0)
 * @length: the length of the bit field
 *
 * Extract from the 16 bit input @value the bit field specified by the
 * @start and @length parameters, and return it. The bit field must
 * lie entirely within the 16 bit word. It is valid to request that
 * all 16 bits are returned (ie @length 16 and @start 0).
 *
 * Returns: the value of the bit field extracted from the input value.
 */
static inline uint16_t extract16(uint16_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 16 - start);
    return extract32(value, start, length);
}

/**
 * extract64:
 * @value: the value to extract the bit field from
 * @start: the lowest bit in the bit field (numbered from 0)
 * @length: the length of the bit field
 *
 * Extract from the 64 bit input @value the bit field specified by the
 * @start and @length parameters, and return it. The bit field must
 * lie entirely within the 64 bit word. It is valid to request that
 * all 64 bits are returned (ie @length 64 and @start 0).
 *
 * Returns: the value of the bit field extracted from the input value.
 */
static inline uint64_t extract64(uint64_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 64 - start);
    return (value >> start) & (~0ULL >> (64 - length));
}


/**
 * deposit64:
 * @value: initial value to insert bit field into
 * @start: the lowest bit in the bit field (numbered from 0)
 * @length: the length of the bit field
 * @fieldval: the value to insert into the bit field
 *
 * Deposit @fieldval into the 64 bit @value at the bit field specified
 * by the @start and @length parameters, and return the modified
 * @value. Bits of @value outside the bit field are not modified.
 * Bits of @fieldval above the least significant @length bits are
 * ignored. The bit field must lie entirely within the 64 bit word.
 * It is valid to request that all 64 bits are modified (ie @length
 * 64 and @start 0).
 *
 * Returns: the modified @value.
 */
static inline uint64_t deposit64(uint64_t value, int start, int length,
                                 uint64_t fieldval)
{
    uint64_t mask;
    assert(start >= 0 && length > 0 && length <= 64 - start);
    mask = (~0ULL >> (64 - length)) << start;
    return (value & ~mask) | ((fieldval << start) & mask);
}



/*
 * Two variations of MIN/MAX macros. The first is for runtime use, and
 * evaluates arguments only once (so it is safe even with side
 * effects), but will not work in constant contexts (such as array
 * size declarations) because of the '{}'.  The second is for constant
 * expression use, where evaluating arguments twice is safe because
 * the result is going to be constant anyway, but will not work in a
 * runtime context because of a void expression where a value is
 * expected.  Thus, both gcc and clang will fail to compile if you use
 * the wrong macro (even if the error may seem a bit cryptic).
 *
 * Note that neither form is usable as an #if condition; if you truly
 * need to write conditional code that depends on a minimum or maximum
 * determined by the pre-processor instead of the compiler, you'll
 * have to open-code it.  Sadly, Coverity is severely confused by the
 * constant variants, so we have to dumb things down there.
 */
#undef MIN
#define MIN(a, b)                                       \
    ({                                                  \
        typeof(1 ? (a) : (b)) _a = (a), _b = (b);       \
        _a < _b ? _a : _b;                              \
    })
#undef MAX
#define MAX(a, b)                                       \
    ({                                                  \
        typeof(1 ? (a) : (b)) _a = (a), _b = (b);       \
        _a > _b ? _a : _b;                              \
    })

#define MAKE_64BIT_MASK(shift, length) \
    (((~0ULL) >> (64 - (length))) << (shift))


#define TARGET_LINX

#ifdef __cplusplus
}
#endif

#endif
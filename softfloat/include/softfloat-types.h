#ifndef SOFTFLOAT_TYPES_H
#define SOFTFLOAT_TYPES_H

#include <stdint.h>
/*
 * Software IEC/IEEE floating-point types.
 */

typedef uint8_t float4;
typedef uint8_t float4_1;
typedef uint8_t float8;
typedef uint8_t float8_1;
typedef uint8_t sf8;
typedef uint16_t float16;
typedef uint32_t float32;
typedef uint64_t float64;
#define float4_val(x) (x)
#define float4_1_val(x) (x)
#define float8_val(x) (x)
#define float8_1_val(x) (x)
#define float16_val(x) (x)
#define float32_val(x) (x)
#define float64_val(x) (x)
#define make_float4(x) (x)
#define make_float4_1(x) (x)
#define make_float8(x) (x)
#define make_float8_1(x) (x)
#define make_float16(x) (x)
#define make_float32(x) (x)
#define make_float64(x) (x)
#define const_float8(x) (x)
#define const_float8_1(x) (x)
#define const_float16(x) (x)
#define const_float32(x) (x)
#define const_float64(x) (x)
typedef struct {
    uint64_t low;
    uint16_t high;
} floatx80;
#define make_floatx80(exp, mant) ((floatx80) { mant, exp })
#define make_floatx80_init(exp, mant) { .low = mant, .high = exp }
typedef struct {
#ifdef HOST_WORDS_BIGENDIAN
    uint64_t high, low;
#else
    uint64_t low, high;
#endif
} float128;
#define make_float128(high_, low_) ((float128) { .high = high_, .low = low_ })
#define make_float128_init(high_, low_) { .high = high_, .low = low_ }

/*
 * Software neural-network floating-point types.
 */
typedef uint16_t bfloat16;

/*
 * Software IEC/IEEE floating-point underflow tininess-detection mode.
 */

#define float_tininess_after_rounding  false
#define float_tininess_before_rounding true

/*
 *Software IEC/IEEE floating-point rounding mode.
 */

typedef enum __attribute__((__packed__)) {
    float_round_nearest_even = 0,
    float_round_down         = 1,
    float_round_up           = 2,
    float_round_to_zero      = 3,
    float_round_ties_away    = 4,
    /* Not an IEEE rounding mode: round to closest odd, overflow to max */
    float_round_to_odd       = 5,
    /* Not an IEEE rounding mode: round to closest odd, overflow to inf */
    float_round_to_odd_inf   = 6,
} FloatRoundMode;

/*
 * Software IEC/IEEE floating-point exception flags.
 */

enum {
    float_flag_invalid   =  1,
    float_flag_divbyzero =  4,
    float_flag_overflow  =  8,
    float_flag_underflow = 16,
    float_flag_inexact   = 32,
    float_flag_input_denormal = 64,
    float_flag_output_denormal = 128
};

/*
 * Rounding precision for floatx80.
 */
typedef enum __attribute__((__packed__)) {
    floatx80_precision_x,
    floatx80_precision_d,
    floatx80_precision_s,
} FloatX80RoundPrec;

/*
 * Floating Point Status. Individual architectures may maintain
 * several versions of float_status for different functions. The
 * correct status for the operation is then passed by reference to
 * most of the softfloat functions.
 */

typedef struct float_status {
    FloatRoundMode float_rounding_mode;
    uint8_t     float_exception_flags;
    FloatX80RoundPrec floatx80_rounding_precision;
    bool tininess_before_rounding;
    /* should denormalised results go to zero and set the inexact flag? */
    bool flush_to_zero;
    /* should denormalised inputs go to zero and set the input_denormal flag? */
    bool flush_inputs_to_zero;
    bool default_nan_mode;
    /*
     * The flags below are not used on all specializations and may
     * constant fold away (see snan_bit_is_one()/no_signalling_nans() in
     * softfloat-specialize.inc.c)
     */
    bool snan_bit_is_one;
    bool use_first_nan;
    bool no_signaling_nans;
} float_status;

#endif /* SOFTFLOAT_TYPES_H */

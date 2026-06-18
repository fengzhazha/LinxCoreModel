#ifndef SOFTFLOAT_HELPERS_H
#define SOFTFLOAT_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "softfloat-types.h"

static inline void set_float_detect_tininess(bool val, float_status *status)
{
    status->tininess_before_rounding = val;
}

static inline void set_float_rounding_mode(FloatRoundMode val,
                                           float_status *status)
{
    status->float_rounding_mode = val;
}

static inline void set_float_exception_flags(int val, float_status *status)
{
    status->float_exception_flags = val;
}

static inline void set_floatx80_rounding_precision(FloatX80RoundPrec val,
                                                   float_status *status)
{
    status->floatx80_rounding_precision = val;
}

static inline void set_flush_to_zero(bool val, float_status *status)
{
    status->flush_to_zero = val;
}

static inline void set_flush_inputs_to_zero(bool val, float_status *status)
{
    status->flush_inputs_to_zero = val;
}

static inline void set_default_nan_mode(bool val, float_status *status)
{
    status->default_nan_mode = val;
}

static inline void set_snan_bit_is_one(bool val, float_status *status)
{
    status->snan_bit_is_one = val;
}

static inline void set_use_first_nan(bool val, float_status *status)
{
    status->use_first_nan = val;
}

static inline void set_no_signaling_nans(bool val, float_status *status)
{
    status->no_signaling_nans = val;
}

static inline bool get_float_detect_tininess(float_status *status)
{
    return status->tininess_before_rounding;
}

static inline FloatRoundMode get_float_rounding_mode(float_status *status)
{
    return status->float_rounding_mode;
}

static inline int get_float_exception_flags(float_status *status)
{
    return status->float_exception_flags;
}

static inline FloatX80RoundPrec
get_floatx80_rounding_precision(float_status *status)
{
    return status->floatx80_rounding_precision;
}

static inline bool get_flush_to_zero(float_status *status)
{
    return status->flush_to_zero;
}

static inline bool get_flush_inputs_to_zero(float_status *status)
{
    return status->flush_inputs_to_zero;
}

static inline bool get_default_nan_mode(float_status *status)
{
    return status->default_nan_mode;
}

#ifdef __cplusplus
}
#endif

#endif /* _SOFTFLOAT_HELPERS_H_ */

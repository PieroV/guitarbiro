/**
 * @file check_float.h
 * @brief Provides some macros to use floating point numbers with check 0.10.
 */

/// The check unit framework
#include <check.h>

/// fabs
#include <math.h>

#ifndef ck_assert_float_eq_tol
#	define ck_assert_float_eq_tol(x, y, t) ck_assert(fabs((x) - (y)) < (t))
#endif

#ifndef ck_assert_float_neq
#	define ck_assert_float_neq(x, y) ck_assert((x) != (y))
#endif

#ifndef ck_assert_double_eq_tol
#	define ck_assert_double_eq_tol(x, y, t) ck_assert(fabs((x) - (y)) < (t))
#endif

#ifndef ck_assert_double_neq
#	define ck_assert_double_neq(x, y) ck_assert((x) != (y))
#endif

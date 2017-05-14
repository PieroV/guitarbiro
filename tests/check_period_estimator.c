/// The check unit framework
#include <check.h>

/// EXIT_SUCCESS, EXIT_FAILURE, malloc, free
#include <stdlib.h>

/// pow, sine, atan, fabs
#include <math.h>

/// The library to test
#include "period_estimator.h"

#include <stdio.h>
#include <assert.h>

/**
 * @brief The frequency of A0 in Hz.
 */
static const float A0 = 27.5;

#ifndef ck_assert_float_eq_tol
/**
 * @brief Macro to use check 0.10 with foats, like check 0.11 does
 */
#	define ck_assert_float_eq_tol(x, y, t) ck_assert(fabs((x) - (y)) < (t))
#endif

#ifndef ck_assert_float_neq
#	define ck_assert_float_neq(x, y) ck_assert((x) != (y))
#endif

/**
 * @brief Compute a frequency starting from A0.
 *
 * @param semitones The semitones above A0 of the note we want to obtain
 *  frequency of
 * @return The frequency
 */
inline static float getFrequency(int semitones)
{
	return A0 * pow(2, semitones / 12.0f);
}

/**
 * @brief Simulate a test with a pure sine and with a sine plus octaves.
 *
 * The test will simulate the sampling rate of 44100.
 */
START_TEST(testPeriodEstimatorSine)
{
	/**
	 * @brief The base frequency for sine tests. In this case A4.
	 *
	 * A4 is 48 semitones above A0 (and should be 440Hz)
	 */
	float f = getFrequency(48);

	ck_assert_msg(fabs(f - 440.0f) < 1e-5, "Non è minore!");
	//ck_assert_float_eq_tol(f, 440.0f, 1e-5);

	/// The sampling rate
	const float fs = 44100;

	/// The Pi
	const float pi = 4 * atan(1);

	/// Lower bound for the frequency detection
	int maxP = (int)floor(fs / A0);

	/**
	 * @brief Upper bound for frequency detection (max frequency = min period)
	 *
	 * This is E7, which is an octave above maximum note of a 24-frets guitar
	 * in standard tuning.
	 * E7 = 6 octaves + 7 semitones (E0 has lower frequency than A0).
	 */
	int minP = (int)ceil(fs / getFrequency(7 * 12 + 7));

	/**
	 * @brief The length of the buffer where the sine signal will be created.
	 *
	 * The estimate_period function requires at least 2 * maxP samples.
	 */
	int len = maxP * 2;

	/// The pure sine test signal
	float *x = malloc(len * sizeof(float));

	/// The sine with octaves signal
	float *y = malloc(len * sizeof(float));

	/// The period of x, in number of elements of x
	float p = fs / f;

	for(int i = 0; i < len; i++) {
		x[i] = sin(2 * pi * i / p);
		y[i] = x[i] + 0.6 * sin(2 * pi * i * 2 / p) +
				0.3 * sin(2 * pi * i * 3 / p);
	}

	float q;
	float estimated = estimatePeriod(x, len, minP, maxP, &q);

	ck_assert_float_neq(estimated, 0);
	estimated = fs / estimated;

	// Allow 0.1% of error
	ck_assert_float_eq_tol(estimated, f, 0.001);
	ck_assert_float_eq_tol(q, 1, 0.05);

	estimated = estimatePeriod(y, len, minP, maxP, &q);
	ck_assert_float_neq(estimated, 0);
	estimated = fs / estimated;
	ck_assert_float_eq_tol(estimated, f, 0.001);
	ck_assert_float_eq_tol(q, 1, 0.05);
}
END_TEST

Suite *periodEstimatorSuite(void)
{
	Suite *s;
	TCase *tcSine;

	s = suite_create("Period estimator");

 	tcSine = tcase_create("Sine");

	tcase_add_test(tcSine, testPeriodEstimatorSine);
	suite_add_tcase(s, tcSine);

	return s;
}

int main()
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = periodEstimatorSuite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

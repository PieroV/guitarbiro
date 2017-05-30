/**
 * @file check_guitar.c
 * @brief Performs unit testing on utilities for musical notes and guitar
 *  utilities.
 */

/// The library to test
#include "guitar.h"

/// The check unit framework
#include <check.h>

/// Macros to use check 0.10 with floating point numbers
#include "check_float.h"

/// EXIT_SUCCESS, EXIT_FAILURE
#include <stdlib.h>

/**
 * @brief Tests the noteToSemitones and frequencyToSemitones functions
 */
START_TEST(testGuitarSemitones)
{
	/// Note names
	char *notes[] = {"C", "C#", "Db", "D", "D#", "Eb", "E", "F",
			"F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B"};

	/**
	 * Hard-coded semitones values for notes of previous array at octave 0 from
	 * A0.
	 */
	semitone_t semitones[] = {-9, -8, -8, -7, -6, -6, -5, -4,
			-3, -3, -2, -1, -1, 0, 1, 1, 2};

	/// Hard-coded frequencies for notes of previous array at octave 0 in Hz
	double freqs[] = {16.35, 17.32, 17.32, 18.35, 19.45, 19.45, 20.60, 21.83,
			23.12, 23.12, 24.50, 25.96, 25.96, 27.50, 29.14, 29.14, 30.87};

	/// The maximum octave to check
	const semitone_t maxOctave = 10;

	/**
	 * The epsilon to accept the frequency error.
	 * Since the values came with 2 significant figures, keep 0.01 as epsilon.
	 */
	const double epsilon = 1e-2;

	size_t len = sizeof(freqs) / sizeof(*freqs);

	for(size_t i = 0; i < len; i++) {
		for(semitone_t octave = 0; octave < maxOctave; octave++) {
			semitone_t fromName = noteToSemitones(notes[i], octave);
			double error;
			semitone_t fromFrequency = frequencyToSemitones(freqs[i], &error);

			ck_assert_int_eq(fromName, semitones[i]);
			ck_assert_int_eq(fromFrequency, semitones[i]);
			ck_assert_double_eq_tol(error, 1, epsilon);

			semitones[i] += 12;
			freqs[i] *= 2;
		}
	}
}
END_TEST

/**
 * @brief Tests the noteToFrets function
 */
START_TEST(testGuitarFrets)
{
	// Check some constants in order for the test to succeed
	ck_assert_int_eq(GUITAR_STRINGS, 6);
	ck_assert_int_eq(GUITAR_FRETS, 22);
	ck_assert_int_eq(STANDARD_TUNING[0], noteToSemitones("E", 4));
	ck_assert_int_eq(STANDARD_TUNING[1], noteToSemitones("B", 3));
	ck_assert_int_eq(STANDARD_TUNING[2], noteToSemitones("G", 3));
	ck_assert_int_eq(STANDARD_TUNING[3], noteToSemitones("D", 3));
	ck_assert_int_eq(STANDARD_TUNING[4], noteToSemitones("A", 2));
	ck_assert_int_eq(STANDARD_TUNING[5], noteToSemitones("E", 2));

	semitone_t tests[] = {
		// Open strings
		STANDARD_TUNING[0],
		STANDARD_TUNING[1],
		STANDARD_TUNING[2],
		STANDARD_TUNING[3],
		STANDARD_TUNING[4],
		STANDARD_TUNING[5],

		// A minor pentatonic scale (wihtout already checked notes)
		noteToSemitones("C", 3),
		noteToSemitones("E", 3),
		noteToSemitones("A", 3),
		noteToSemitones("C", 4),
		noteToSemitones("D", 4),
		noteToSemitones("G", 4),
		noteToSemitones("A", 4),
		noteToSemitones("C", 5),
	};

	semitone_t expected[][/*GUITAR_STRINGS*/6] = {
		{0, 5, 9, 14, 19, -1},
		{-1, 0, 4, 9, 14, 19},
		{-1, -1, 0, 5, 10, 15},
		{-1, -1, -1, 0, 5, 10},
		{-1, -1, -1, -1, 0, 5},
		{-1, -1, -1, -1, -1, 0},

		{-1, -1, -1, -1, 3, 8},
		{-1, -1, -1, 2, 7, 12},
		{-1, -1, 2, 7, 12, 17},
		{-1, 1, 5, 10, 15, 20},
		{-1, 3, 7, 12, 17, 22},
		{3, 8, 12, 17, 22, -1},
		{5, 10, 14, 19, -1, -1},
		{8, 13, 17, 22, -1, -1},
	};

	size_t numTests = sizeof(tests) / sizeof(*tests);
	for(size_t i = 0; i < numTests; i++) {
		semitone_t frets[6];
		noteToFrets(tests[i], STANDARD_TUNING, frets, 6, GUITAR_FRETS);

		for(unsigned int j = 0; j < GUITAR_STRINGS; j++) {
			if(expected[i][j] >= 0) {
				ck_assert_int_eq(frets[j], expected[i][j]);
			} else {
				ck_assert_int_lt(frets[j], 0);
			}
		}
	}
}
END_TEST

Suite *guitarSuite(void)
{
	Suite *s;
	TCase *tcSemitones;
	TCase *tcFrets;

	s = suite_create("Guitar");

 	tcSemitones = tcase_create("noteToSemitones and frequencyToSemitones");
	tcase_add_test(tcSemitones, testGuitarSemitones);
	suite_add_tcase(s, tcSemitones);

	tcFrets = tcase_create("noteToFrets");
	tcase_add_test(tcFrets, testGuitarFrets);
	suite_add_tcase(s, tcFrets);

	return s;
}

int main()
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = guitarSuite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

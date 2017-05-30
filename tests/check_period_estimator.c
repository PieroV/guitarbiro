/**
 * @file check_period_estimator.c
 * @brief Performs unit testing on estimatePeriod function
 */

/// The check unit framework
#include <check.h>

/// The library to test
#include "period_estimator.h"

/// EXIT_SUCCESS, EXIT_FAILURE, malloc, free
#include <stdlib.h>

/// fprintf
#include <stdio.h>

/// pow, sine, atan, fabs, ceil, floor
#include <math.h>

/// strlen, strcat
#include <string.h>

/// Macros to use check 0.10 with floating point numbers
#include "check_float.h"

/// noteToSemitones
#include "guitar.h"

/**
 * @brief The frequency of A0 in Hz.
 */
static const double A0 = 27.5;

/**
 * @brief The timeout to run the "real world samples" test.
 *
 * This test is run with samples of about 5 seconds (which are 220500 floats),
 * so it requires some time to be performed, more than the default 4 seconds
 * that check allows.
 *
 * Just for reference on Intel Core i7 4700HQ it requires about 13.5 seconds, on
 * an Intel Core i3 2120 it requires about 15.5 seconds.
 */
static const double SAMPLES_TIMEOUT = 60.0;

/**
 * @brief The path to the audio samples that will be used for testing.
 *
 * Since check doesn't provide a nice way to pass arguments to test functions,
 * we'll have to use this global variable to store the path to audio samples.
 */
static char *gPath;

/**
 * @brief Compute a frequency starting from A0.
 *
 * @param semitones The semitones above A0 of the note we want to obtain
 *  frequency of
 * @return The frequency
 */
inline static double getFrequency(semitone_t semitones);

/**
 * @brief Open a sample file.
 *
 * @param filename The name of the file of the sample. The path will be
 *  automatically added using gPath variable
 * @param size Output param that will contain the size of the sample (measured
 *  in number of elements, not in byte)
 * @return The buffer. The caller will have to free it
 */
static float *openSample(const char *filename, size_t *size);

/**
 * @brief Simulate a test with a pure sine signal and with a sine plus octaves.
 *
 * The test will simulate the sampling rate of 44100.
 */
START_TEST(testPeriodEstimatorSine)
{
	/**
	 * @brief The base frequency for sine tests. In this case A4.
	 */
	double f = getFrequency(noteToSemitones("A", 4));

	ck_assert_double_eq_tol(f, 440.0, 1e-5);

	/// The sampling rate
	const int fs = 44100;

	/// The Pi
	const double pi = 4 * atan(1);

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
	double p = fs / f;

	for(int i = 0; i < len; i++) {
		x[i] = sin(2 * pi * i / p);
		y[i] = x[i] + 0.6 * sin(2 * pi * i * 2 / p) +
				0.3 * sin(2 * pi * i * 3 / p);
	}

	double q;
	double estimated = estimatePeriod(x, len, minP, maxP, &q);

	ck_assert_double_neq(estimated, 0);
	estimated = fs / estimated;

	// Allow 0.1% of error
	ck_assert_double_eq_tol(estimated, f, 0.001);
	ck_assert_double_eq_tol(q, 1, 0.05);

	estimated = estimatePeriod(y, len, minP, maxP, &q);
	ck_assert_double_neq(estimated, 0);
	estimated = fs / estimated;
	ck_assert_double_eq_tol(estimated, f, 0.001);
	ck_assert_double_eq_tol(q, 1, 0.05);
}
END_TEST

/**
 * @brief Test estimatePeriod using real world samples.
 */
START_TEST(testPeriodEstimatorSamples)
{
	/// The sample rate of the samples
	const int rate = 44100;

	/// The name of the samples to check
	const char *samples[] = {
		"A2_string5.pcm",
		"A4_string1.pcm",
		"B3_string2.pcm",
		"D3_string4.pcm",
		"E2_string6.pcm",
		"E4_string1.pcm",
		"G3_string3.pcm",
		""
	};

	/// The expected semitones from A0 of samples
	semitone_t expected[] = {
		noteToSemitones("A", 2),
		noteToSemitones("A", 4),
		noteToSemitones("B", 3),
		noteToSemitones("D", 3),
		noteToSemitones("E", 2),
		noteToSemitones("E", 4),
		noteToSemitones("G", 3),
		0 // This one is just a placeholder to avoid segmentation faults
	};

	/// The max period (minimum frequency) that we want to be able to detect
	int maxPeriod = (int) ceil(44100 / getFrequency(noteToSemitones("A", 0)));
	/**
	 * The min period (maximum frequency) that we want to be able to detect.
	 *
	 * E7 is a good period because it's an octave above the maximum note that
	 * can be made in a standard-tuned 24-frets guitar.
	 */
	int minPeriod = (int) floor(44100 / getFrequency(noteToSemitones("E", 7)));

	for(int i = 0; strlen(samples[i]); i++) {
		size_t size;
		double freq;
		double quality;

		// openSample already checks for errors, so don't check again
		float *buf = openSample(samples[i], &size);

		freq = rate / estimatePeriod(buf, size, minPeriod, maxPeriod, &quality);
		ck_assert_int_eq(frequencyToSemitones(freq, 0), expected[i]);

		free(buf);
	}
}
END_TEST

/**
 * @brief Create the suite to check estimatePeriod
 * @return The test suite
 */
Suite *periodEstimatorSuite()
{
	Suite *s;
	TCase *tcSine;
	TCase *tcSamples;

	s = suite_create("Period estimator");

 	tcSine = tcase_create("Sine");
	tcase_add_test(tcSine, testPeriodEstimatorSine);
	suite_add_tcase(s, tcSine);

	tcSamples = tcase_create("Real world samples");
	tcase_add_test(tcSamples, testPeriodEstimatorSamples);
	tcase_set_timeout(tcSamples, SAMPLES_TIMEOUT);
	suite_add_tcase(s, tcSamples);

	return s;
}

/**
 * @brief The entry point for these tests.
 *
 * This program requires, as command line argument, the path to the directory
 * containing the samples that will be used for some tests.
 *
 * @param argc The number of command line arguments
 * @param argv The command line arguments
 * @return The exit status code
 */
int main(int argc, char *argv[])
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	if(argc < 2) {
		fprintf(stderr, "The program requires one argument.\n");
		return EXIT_FAILURE;
	}
	gPath = argv[1];

	s = periodEstimatorSuite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

inline static double getFrequency(semitone_t semitones)
{
	return A0 * pow(2, semitones / 12.0f);
}

static float *openSample(const char *filename, size_t *size)
{
	#ifdef WIN32
		const char *slash = "\\";
	#else
		const char *slash = "/";
	#endif

	/// The sample file handler
	FILE *fp;
	/// The file path
	char *realName;
	/// The file size
	size_t fileSize;
	/// The pointer buffer for the sample that will be returned
	float *buf;


	// + 2 = slash + null char
	realName = calloc(strlen(filename) + strlen(gPath) + 2, sizeof(char));
	strcat(realName, gPath);
	strcat(realName, slash);
	strcat(realName, filename);

	fp = fopen(realName, "rb");
	free(realName);
	realName = 0;

	ck_assert_msg(fp, "Could not open sample file %s.", filename);

	// The most portable way to get a file size...
	fseek(fp, 0L, SEEK_END);
	fileSize = ftell(fp);
	rewind(fp);

	/* Set the buffer size before the reading, therefore if the latter fails
	we can set size to 0. */
	ck_assert_int_eq(fileSize % sizeof(float), 0);
	*size = fileSize / sizeof(float);

	buf = (float *) malloc(fileSize);
	if(fread(buf, 1, fileSize, fp) != fileSize) {
		ck_abort_msg("An error occurred while reading %s.", filename);
		free(buf);
		buf = 0;
		*size = 0;
	}

	fclose(fp);

	return buf;
}

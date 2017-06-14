/**
 * @file period_estimator.c
 * @brief Computes the period of a signal using normalized autocorrelation.
 */

 /*
 The MIT License (MIT)

 Copyright (c) 2009 Gerald T Beauregard

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

/// free, malloc, realloc
#include <stdlib.h>

/// fprintf
#include <stdio.h>

/// sqrt, isnan
#include <math.h>

/// assert
#include <assert.h>

/**
 * @brief The buffer that contains the normalized auto correlation.
 *
 * This buffer is allocated each time it's needed by estimatePeriod, but to
 * speed further callings, it's not deallocated.
 * It has to be freed by estimateFree.
 */
static double *gNac = 0;

/**
 * @brief The length of gNac (as number of elements, not as bytes).
 */
static size_t gLength = 0;

/**
 * @brief Allocate the buffer for the auto correlation.
 *
 * @param size The size needed for the buffer
 * @return The address of the buffer. Even though it's contained in the global
 *  variable gNac, we prefer to return it in case we remove the global variable.
 */
static double *alloc(size_t size);

/**
 * @brief Computes the normalized auto correlation.
 *
 * The normalization is such that if the signal is perfectly periodic with
 * (integer) period p, the NAC will be exactly 1.0.
 * NAC is also exactly 1.0 for periodic signal with exponential decay or
 * increase in magnitude.
 *
 * @param x The signal
 * @param n The number of samples in the signal
 * @param minP The minimum period of interest
 * @param maxP The maximum period of interest
 * @param nac The array of the correlation
 */
static void computeNac(const float *x, int n, int minP, int maxP, double *nac);

/**
 * @brief Find the peak of the auto correlation in the range of interest.
 *
 * @param nac The array of the correlation
 * @param minP The minimum period of interest
 * @param maxP The maximum period of interest
 * @param period The estimated period from interpolation
 * @return The index of the element with the maximum auto correlation
 */
static int findPeak(const double *nac, int minP, int maxP, double *period);

/**
 * @brief Check for and correct the octave errors.
 *
 * If the range of pitches being searched is greater than one octave, the basic
 * algo above may make "octave" errors, in which the period identified is
 * actually some integer multiple of the real period. (Makes sense, as a signal
 * that's periodic with period p is technically also period with period 2p).
 *
 * Algorithm is pretty simple: we hypothesize that the real period is some
 * "submultiple" of the "bestP" above. To check it, we see whether the NAC is
 * strong at each of the hypothetical subpeak positions.
 * E.g. if we think the real period is at 1/3 our initial estimate, we check
 * whether the NAC is strong at 1/3 and 2/3 of the original period estimate.
 *
 * @param nac The array of the correlation
 * @param minP The minimum period of interest
 * @param period The estimated period
 * @param maxNac The index of the element that has maximum auto correlation
 * @return The (eventually) changed period
 */
static double fixOctaves(const double *nac, int minP, double period, int maxNac);

double estimatePeriod(const float *x, int n, int minP, int maxP, double *q,
		int *periodInt)
{
	assert(minP > 1);
	assert(maxP > minP);
	assert(n >= 2*maxP);
	assert(x != NULL);
	assert(q);

	/// The period of the signal
	double period = 0.0;

	/**
	 * @brief The index of the element that has maximum auto-correlation.
	 *
	 * This could be different from period because the latter is an
	 * interpolation from periodInt and its neighbourgs.
	 */
	int maxNac;

	/// The buffer to store normalized auto correlation into
	double *nac;

	*q = 0;

	/* Size is maxP + 2 (not maxP + 1) because we need up to element maxP + 1 to
	check whether element at maxP is a peak.
	Thanks to Les Cargill for spotting the bug. */
	if(!(nac = alloc(maxP + 2))) {
		fprintf(stderr, "Could not allocate the buffer for the autocorrelation.\n");
		*q = -1.0;
		return 0;
	}

	computeNac(x, n, minP, maxP, nac);

	maxNac = findPeak(nac, minP, maxP, &period);
	if(maxNac == -1) {
		return 0.0;
	}

	/* "Quality" of periodicity is the normalized autocorrelation at the best
	period (which may be a multiple of the actual period). */
	*q = nac[maxNac];

	if(periodInt) {
		*periodInt = maxNac;
	}

	period = fixOctaves(nac, minP, period, maxNac);

	return period;
}

static double *alloc(size_t size)
{
	if(gNac == 0) {
		gNac = (double *) malloc(size * sizeof(double));
		gLength = size;
	} else if(gLength < size) {
		gNac = (double *) realloc(gNac, size * sizeof(double));
		gLength = size;
	}

	return gNac;
}

static void computeNac(const float *x, int n, int minP, int maxP, double *nac)
{
	/// Sum of squares of beginning part
	double sumSqBeg = 0.0;
	/**
	 * @brief Sum of squares of ending part
	 *
	 * The initial value is needed because at first for iteration, it will
	 * subtracted.
	 * However minP - 2 is always a legal index because minP >= 2 (see asserts
	 * on estimatePeriod).
	 */
	 double sumSqEnd = x[minP - 2] * x[minP - 2];

	for(int i = 0, j = minP - 1; i < n - minP + 1; i++, j++) {
		sumSqBeg += x[i] * x[i];
		sumSqEnd += x[j] * x[j];
	}

	for(int p = minP - 1; p <= maxP + 1; p++) {
		/// Standard auto-correlation
		double ac = 0.0;

		for(int i = 0; i < n - p; i++) {
			ac += x[i] * x[i + p];
		}

		sumSqBeg -= x[n - p] * x[n - p];
		sumSqEnd -= x[p - 1] * x[p - 1];

		if(sumSqBeg != 0 && sumSqEnd != 0) {
			nac[p] = ac / sqrt(sumSqBeg * sumSqEnd);
		} else {
			nac[p] = 0;
		}
	}
}

static int findPeak(const double *nac, int minP, int maxP, double *period)
{
	/**
	 * @brief The maximum relative error to accept the frequency interpolation.
	 *
	 * Once the peak is found, there's a linear interpolation between the peak
	 * and the previous and following values of autocorrelation.
	 *
	 * However if these three values are too near, the shift explodes, and
	 * during tests this lead to negative frequencies in some cases!
	 * So if the shift value is too high, we just ignore it.
	 */
	const double shiftMaxError = 0.2;

	/// The return value
	int best = minP;

	for(int p = minP; p <= maxP; p++) {
		if(nac[p] > nac[best]) {
			best = p;
		}
	}

	/*  Give up if it's highest value, but not actually a peak.
	This can happen if the period is outside the range [minP, maxP] */
	if(nac[best] < nac[best - 1] && nac[best] < nac[best + 1]) {
		return -1;
	}

	/* Interpolate based on neighboring values.
	E.g. if value to right is bigger than value to the left, real peak is a bit
	to the right of discretized peak.
	if left  == right, real peak = mid;
	if left  == mid,   real peak = mid - 0.5
	if right == mid,   real peak = mid + 0.5 */
	double mid   = nac[best];
	double left  = nac[best - 1];
	double right = nac[best + 1];

	*period = best;

	if(2 * mid - left - right != 0.0) {
		double shift = 0.5 * (right - left) / ( 2 * mid - left - right );

		if(fabs(shift) < shiftMaxError * best) {
			*period = best + shift;
		}
	}
	/* else mid == (left + right) / 2 => no shift required and "best" is already
	the best period. */

	/*
	 * Some ill formed signals can contain NaN values.
	 * This could happen, on guitar signals, when cables are defective and they
	 * produce high power noises, or when mechanical switches (pickup selector,
	 * volume potentiometer...) are used.
	 *
	 * During tests we encountered some situations of this kind, and a peak was
	 * found correctly, but since the signal is corrupted, we prefer returning
	 * an error.
	 */
	if(isnan(*period)) {
		*period = 0.0;
		return -1;
	}

	return best;
}

static double fixOctaves(const double *nac, int minP, double period, int maxNac)
{
	/**
	 * @brief Threshold to detect the real period.
	 *
	 * If strength at all submultiple of peak pos are this strong relative to
	 * the peak, assume the submultiple is the real period.
	 */
	const double k_subMulThreshold = 0.90;

	//  For each possible multiple error (starting with the biggest)
	int maxMul = maxNac / minP;
	int found = 0;
	for (int mul = maxMul; !found && mul >= 1; mul--) {
		// Check whether all submultiples of original peak are nearly as strong
		int subsAllStrong = 1;

		//  For each submultiple
		for(int k = 1; k < mul; k++) {
			int subMulP = (int)(k * period / mul + 0.5);
			/* If it's not strong relative to the peak NAC, then not all
			submultiples are strong, so we haven't found the correct
			submultiple. */
			if(nac[subMulP] < k_subMulThreshold * nac[maxNac]) {
				subsAllStrong = 0;
			}

			/**
			 * @todo: Use spline interpolation to get better estimates of NAC
			 * magnitudes for non-integer periods in the above comparison
			 */
		}

		/* If yes, then we're done.
		New estimate of period is "submultiple" of original period. */
		if(subsAllStrong) {
			found = 1;
			period = period / mul;
		}
	}

	return period;
}

void estimateFree()
{
	if(gNac) {
		free(gNac);
		gNac = 0;
		gLength = 0;
	}
}

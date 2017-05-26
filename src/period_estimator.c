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

/// sqrt
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
static float *gNac = 0;

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
static float *alloc(size_t size);

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
static void computeNac(const float *x, int n, int minP, int maxP, float *nac);

/**
 * @brief Find the peak of the auto correlation in the range of interest.
 *
 * @param nac The array of the correlation
 * @param minP The minimum period of interest
 * @param maxP The maximum period of interest
 * @param period The estimated period from interpolation
 * @return The index of the element with the maximum auto correlation
 */
static int findPeak(const float *nac, int minP, int maxP, float *period);

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
static float fixOctaves(const float *nac, int minP, float period, int maxNac);

float estimatePeriod(const float *x, int n, int minP, int maxP, float *q)
{
	assert(minP > 1);
	assert(maxP > minP);
	assert(n >= 2*maxP);
	assert(x != NULL);
	assert(q);

	/// The period of the signal
	float period = 0.0;

	/**
	 * @brief The index of the element that has maximum auto-correlation.
	 *
	 * This could be different from period because the latter is an
	 * interpolation from periodInt and its neighbourgs.
	 */
	int maxNac;

	/// The buffer to store normalized auto correlation into
	float *nac;

	*q = 0;

	/* Size is maxP + 2 (not maxP + 1) because we need up to element maxP + 1 to
	check whether element at maxP is a peak.
	Thanks to Les Cargill for spotting the bug. */
	if(!(nac = alloc(maxP + 2))) {
		fprintf(stderr, "Could not allocate the buffer for the autocorrelation.\n");
		*q = -1.0f;
		return 0;
	}

	computeNac(x, n, minP, maxP, nac);

	maxNac = findPeak(nac, minP, maxP, &period);
	if(maxNac == -1) {
		return 0.0f;
	}

	/* "Quality" of periodicity is the normalized autocorrelation at the best
	period (which may be a multiple of the actual period). */
	*q = nac[maxNac];

	period = fixOctaves(nac, minP, period, maxNac);

	return period;
}

static float *alloc(size_t size)
{
	if(gNac == 0) {
		gNac = (float *) malloc(size * sizeof(float));
		gLength = size;
	} else if(gLength < size) {
		gNac = (float *) realloc(gNac, size * sizeof(float));
		gLength = size;
	}

	return gNac;
}

static void computeNac(const float *x, int n, int minP, int maxP, float *nac)
{
	for(int p = minP - 1; p <= maxP + 1; p++) {
		/// Standard auto-correlation
		float ac = 0.0;
		/// Sum of squares of beginning part
		float sumSqBeg = 0.0;
		/// Sum of squares of ending part
		float sumSqEnd = 0.0;

		for(int i = 0; i < n - p; i++) {
			ac += x[i] * x[i + p];
			sumSqBeg += x[i] * x[i];
			sumSqEnd += x[i+p] * x[i+p];
		}

		if(sumSqBeg != 0 && sumSqEnd != 0) {
			nac[p] = ac / sqrt(sumSqBeg * sumSqEnd);
		} else {
			nac[p] = 0;
		}
	}
}

static int findPeak(const float *nac, int minP, int maxP, float *period)
{
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
	float mid   = nac[best];
	float left  = nac[best - 1];
	float right = nac[best + 1];

	if(2 * mid - left - right != 0.0f) {
		float shift = 0.5 * (right - left) / ( 2 * mid - left - right );
		*period = best + shift;
	} else {
		// mid == (left + right) / 2 => no shift required
		*period = best;
	}

	return best;
}

static float fixOctaves(const float *nac, int minP, float period, int maxNac)
{
	/**
	 * @brief Threshold to detect the real period.
	 *
	 * If strength at all submultiple of peak pos are this strong relative to
	 * the peak, assume the submultiple is the real period.
	 */
	const float k_subMulThreshold = 0.90;

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

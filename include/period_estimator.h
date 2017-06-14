/**
 * @file period_estimator.h
 * @brief Computes the period of a signal using normalized autocorrelation.
 */

#ifndef __PERIOD_ESTIMATOR_H
#define __PERIOD_ESTIMATOR_H

/**
 * @brief Estimate the period of a signal.
 *
 * A period estimator algorithm based on normalized autocorrelation.
 * It also includes sub-sample accuracy of the period estimate, and avoidance of
 * octave errors.
 *
 * @note This function uses periods. Frequencies and periods are bound by this
 *  formula: f = Fs / T, being f the frequency, T the period and Fs the sampling
 *  rate.
 *
 * @author Gerry Beauregard <gerry.beauregard@gmail.com>
 * @link https://gerrybeauregard.wordpress.com/2013/07/15/high-accuracy-monophonic-pitch-estimation-using-normalized-autocorrelation/
 * @copyright MIT License
 *
 * @param x The signal
 * @param n The number of samples. It must be at least 2*maxP.
 * @param minP Minimum period of interest
 * @param Maximum period of interest
 * @param q Quality of the periodicity (1 = perfectly periodic). This parameter
 *  cannot be null and must be correctly allocated
 * @param periodInt Output parameter that if not null will have contain the
 *  period of the signal, obtained from the normalized autocorrelation without
 *  any interpolation or fix (as the octaves one!). If null it will be ignored
 * @return The period of signal (in number of elements of x array)
 */
double estimatePeriod(const float *x, int n, int minP, int maxP, double *q,
		int *periodInt);

/**
 * @brief Free the buffer used by estimatePeriod.
 *
 * estimatePeriod needs a buffer to compute data.
 * To allow its usage in realtime, the buffer is created once and kept for
 * further calls, so this is the function which allows its deallocation.
 */
void estimateFree();

#endif /* __PERIOD_ESTIMATOR_H */

/*
The MIT License (MIT)

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

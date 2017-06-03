/**
 * @file detect.c
 * @brief Detect which note has been played in a sequency of audio samples.
 */

#include "detect.h"

/// semitone_t, noteToFrequency, GUITAR_STRINGS
#include "guitar.h"

/// estimatePeriod
#include "period_estimator.h"

/// malloc, free
#include <stdlib.h>

/// floor, ceil
#include <math.h>

/// assert
#include <assert.h>

#include <stdio.h>

struct _DetectContext {
	/**
	 * @brief The sample rate
	 *
	 * It's needed to get the actual frequency of data.
	 */
	unsigned int rate;

	/**
	 * @brief The minium period of the signal in samples.
	 * @sa estimatePeriod
	 */
	int minPeriod;

	/**
	 * @brief The maximum period of the signal in samples.
	 * @sa estimatePeriod
	 */
	int maxPeriod;

	/**
	 * @brief The last note that has benn detected.
	 */
	semitone_t lastDetected;
};

DetectContext *detectInit(unsigned int rate)
{
	if(!rate) {
		return 0;
	}

	/// The instance of DetectContext that will be returned
	DetectContext *ret = (DetectContext *) malloc(sizeof(DetectContext));

	ret->rate = rate;

	// Note: highest note/frequency = minimum period and vice versa
	ret->minPeriod = (int) floor(rate / noteToFrequency(DETECT_HIGHEST));
	ret->maxPeriod = (int) ceil(rate / noteToFrequency(DETECT_LOWEST));

	ret->lastDetected = INVALID_SEMITONE;

	return ret;
}

void detectFree(DetectContext *context)
{
	if(!context) {
		return;
	}

	free(context);
}

int detectAnalyze(DetectContext *context, struct SoundIoRingBuffer *buffer)
{
	assert(context);
	assert(context->minPeriod > 0);
	assert(context->maxPeriod > context->minPeriod);

	/// Threshould to accept notes
	const double qThres = 0.98;

	/// The number of available samples (in bytes)
	int availableBytes;
	/// The number of available samples (in floats)
	int available;
	/// The data buffer
	float *buf;
	/// The array to store in which frets the note can be played
	semitone_t frets[GUITAR_STRINGS];
	/// The frequency of the note
	double freq;
	/// The periodicity quality
	double quality;
	/// Average amplitude of the signal, used for a basic noise filter
	double avg = 0;

	availableBytes = soundio_ring_buffer_fill_count(buffer);
	available = availableBytes / sizeof(float);

	// Not enough samples to detect frequency
	if(available < (2 * context->maxPeriod)) {
		return 0;
	}

	buf = (float *) soundio_ring_buffer_read_ptr(buffer);

	for(int i = 0; i < available; i++) {
		avg += buf[i];
	}
	avg /= available;

	if(avg > -.85) {
		freq = estimatePeriod(buf, available, context->minPeriod,
				context->maxPeriod, &quality);
		if(freq <= 0) {
			printf("Skip due to non positive frequency (%f)\n", freq);
		} else if(quality < qThres) {
			printf("Skip due to too low quality (f: %f, q: %f)\n", freq, quality);
		} else {
			freq = context->rate / freq;
			semitone_t note = frequencyToSemitones(freq, 0);

			if(note != context->lastDetected) {
				printf("New note: %hd (f: %f)\n", note, freq);
				context->lastDetected = note;
			}

			printf("F: %f, q: %f, a: %f\n", freq, quality, avg);
		}
	}

	soundio_ring_buffer_advance_read_ptr(buffer, availableBytes);

	return 0;
}

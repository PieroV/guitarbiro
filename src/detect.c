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

/// floor, ceil, isfinite
#include <math.h>

/// assert
#include <assert.h>

/// printf
#include <stdio.h>

/// guiHighlightFrets, guiResetHighlights
#include "gui.h"

/**
 * @brief Enable printing information about data filtering?
 */
#define DEBUG_FILTERING 0

#if DEBUG_FILTERING
#	define FILTER_PRINTF(...) printf(__VA_ARGS__)
#else
#	define FILTER_PRINTF(...) ;
#endif

/**
 * @brief The number of amplitude peaks to keep in memory.
 *
 *
 */
#define PEAKS_SIZE 100

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

	/**
	 * @brief Last peaks.
	 * @sa PEAKS_SIZE
	 *
	 * This array is intended to be used as a circular array jointly with
	 * lastPeak.
	 * For the logic of the program, even when not peaks have been saved,
	 * considering them as 0 is a valid behaviour.
	 */
	double peaks[PEAKS_SIZE];

	/**
	 * @brief Index of used last element of peaks
	 * @sa peaks
	 */
	unsigned int lastPeak;

	/**
	 * @brief Samples filtered out from last update.
	 *
	 * We track the number of samples that we filter out to reset the last
	 * played note if too many samples were discarted because they were
	 * recognized as noise.
	 *
	 * Setting no valid note counts as an update.
	 */
	unsigned int droppedSamples;
};

/**
 * @brief Threshold of periodicity quality to accept notes
 *
 * A first filter to distinguish noise from signal is based on the periodicity
 * quality reported by estimatePeriod
 */
static const double MINIMUM_QUALITY = 0.85;

/**
 * @brief Threshold for signal amplitude
 *
 * A second filter concerns the signal amplitude: guitar pickups (especially
 * single coils) and not properly filtered sound cards add some noise to signal,
 * so if no sample exceeds this threshold, then that sequcence is treated as
 * silence.
 *
 * @note This threshold is applied to absolute value of the amplitude.
 */
static const double NOISE_THRESHOLD = 0.1;

/**
 * @brief Threshold to detect note replay.
 *
 * The only way to detect if a note has been played again (and to detect false
 * octave changes) is checking amplitude changes.
 * As a matter of fact, when a note is played again, the amplitude has a quick
 * fall and raise.
 * This parameter allows to define a threshold of how "quick" this raise should
 * be.
 *
 * This parameter has been adjusted during tests.
 */
static const double RAISE_THRESHOLD = 0.12;

/**
 * @brief Performs the analysis on already filtered signal.
 *
 * detectAnalyze has to do some checks on the signal, and so it's used as a
 * public interface, but this is the function that performs the analysis on
 * signals that have already been filtered.
 *
 * @param context An instance of DetectContext
 * @param buf The buffer of samples
 * @param size The size of the buffer
 * @param freq The frequency of the buffer
 * @param period The period of the buffer as number of elements
 */
static void analyzeFiltered(DetectContext *context, float *buf, int size,
		double freq, int period);

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

	for(int i = 0; i < PEAKS_SIZE; i++) {
		ret->peaks[i] = 0;
	}
	// First element will be ((PEAKS_SIZE - 1) + 1) % PEAKS_SIZE = 0
	ret->lastPeak = PEAKS_SIZE - 1;

	ret->droppedSamples = 0;

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

	/// The number of available samples (in bytes)
	int availableBytes;
	/// The number of available samples (in floats)
	int available;
	/// The data buffer
	float *buf;
	/// The period of the note (in sample units)
	double period;
	/// The periodicity quality
	double quality;
	/// The period as integer
	int intPeriod;

	availableBytes = soundio_ring_buffer_fill_count(buffer);
	available = availableBytes / sizeof(float);

	// Not enough samples to detect frequency
	if(available < (2 * context->maxPeriod)) {
		return 0;
	}

	if(context->droppedSamples > context->rate) {
		// A second of noise or spurious data is enogh to make the note invalid
		guiResetHighlights();
		context->lastDetected = INVALID_SEMITONE;
		context->droppedSamples = 0;
	}

	buf = (float *) soundio_ring_buffer_read_ptr(buffer);
	period = estimatePeriod(buf, available, context->minPeriod,
			context->maxPeriod, &quality, &intPeriod);

	// First filter: skip signals with negative period and low periodicity
	if(isfinite(period) && intPeriod > 0 && quality >= MINIMUM_QUALITY) {
		double freq = context->rate / period;
		analyzeFiltered(context, buf, available, freq, intPeriod);
	} else {
		context->droppedSamples += available;
		FILTER_PRINTF("Negative period or insufficient quality! T: %f Q: %f\n",
				period, quality);
	}

	soundio_ring_buffer_advance_read_ptr(buffer, availableBytes);

	return 0;
}

void analyzeFiltered(DetectContext *context, float *buf, int size, double freq,
		int period)
{
	/* The function should be called only from detectAnalyze, so data should
	have already been checked, but let's check them anyway in debug stage. */
	assert(size > 0);
	assert(freq > 0);
	assert(period > 0);

	/// The array to store in which frets the note can be played
	semitone_t frets[GUITAR_STRINGS];
	/// The note that has been played
	semitone_t note = frequencyToSemitones(freq, 0);
	/// The difference, in semitones from the previous played note
	semitone_t noteDelta;
	/// Tells if NOISE_THRESHOLD has been surpassed
	char minSurpassed = 0;
	/// Tells if a quick raise has happened
	char quickRaise = 0;

	if(!noteToFrets(note, STANDARD_TUNING, frets, GUITAR_STRINGS, GUITAR_FRETS)) {
		FILTER_PRINTF("Non playable note (%hd)...\n", note);
		context->droppedSamples += size;
		return;
	}

	/* All the tests on signal amplitude exploit the fact that the signal is
	periodic: this should allow to check a feature only on the peak of a period
	(that is true, for example, for amplitude decay, or for minumum threshold to
	pass not to be detected as a silent signal). */
	for(int j = 0, limit = size - period; j < limit; j += period) {
		double peak = 0;

		for(int i = 0; i < period; i++) {
			double tmp = fabs(buf[j + i]);
			if(tmp > peak) {
				peak = tmp;
			}
		}

		/* Detect quick raise. Note that if a quick raise has already been
		detected, then all further checks will be skipped. */
		quickRaise = quickRaise ||
				peak - context->peaks[context->lastPeak] > RAISE_THRESHOLD;

		context->lastPeak++;
		context->lastPeak %= PEAKS_SIZE;
		context->peaks[context->lastPeak] = peak;

		minSurpassed = minSurpassed || peak > NOISE_THRESHOLD;
	}

	/* At this point we will for sure detect silence or a valid note, so, even
	if we have a note repetition, we take it as an update and reset the
	droppedSamples counter. */
	context->droppedSamples = 0;

	if(!minSurpassed) {
		guiResetHighlights();
		// In this way the next note will always be detected as a new one
		context->lastDetected = INVALID_SEMITONE;
		FILTER_PRINTF("No minium threshold on amplitude!\n");
		return;
	}

	noteDelta = abs(note - context->lastDetected) % 12;
	if(quickRaise || (noteDelta != 0 && noteDelta != 7) ||
			context->lastDetected == INVALID_SEMITONE) {
		/*printf("New note: %hd (f: %f)\n", note, freq);
		printf("Frets: %hd %hd %hd %hd %hd %hd\n", frets[0], frets[1],
				frets[2], frets[3], frets[4], frets[5]);*/
		guiHighlightFrets(frets);
		context->lastDetected = note;
	}
}

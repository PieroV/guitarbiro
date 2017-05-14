/**
 * @file audio_record.c
 * @brief Manages the acquisition of samples from the audio card.
 *
 * @link http://libsound.io/doc-1.1.0/sio_list_devices_8c-example.html
 */

#include "audio.h"

/// printf, scanf, fprintf
#include <stdio.h>
/// malloc, free
#include <stdlib.h>
/// strerror, memset, memcpy
#include <string.h>
/// errno
#include <errno.h>
/// assert
#include <assert.h>

#ifdef WIN32
	/// Sleep
#	include <windows.h>
#elif defined(__linux__)
	/// usleep (on Linux)
#	include <unistd.h>
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
	/// nanosleep
#	include <time.h>
#endif

/**
 * @brief The sample rates we accept from the sound card.
 *
 * For our purposes we can use standard sample rates, we don't need high sample
 * rates that would only use too memory.
 * So this is the list of the sample reates that we would like to use, in
 * preference order.
 *
 * Last rate must be 0 as a condition to stop the iteration on this array.
 */
static const int SAMPLE_RATES[] = {
	44100,
	48000,
	96000,
	24000,
	0,
};

/**
 * @brief The formats we accept from the sound card.
 *
 * Probably the frequency algorithm will need float data, since we cannot make
 * a template in C. So we will accept only this format of data, in little endian
 * or bit endian.
 *
 * @todo Check description when we will have the algorithm
 */
static const enum SoundIoFormat FORMATS[] = {
	SoundIoFormatFloat32LE,
	SoundIoFormatFloat32BE,
	SoundIoFormatInvalid,
};

/**
 * @brief How much space allocate for the temporary buffer, measured in seconds.
 */
static const int RING_BUFFER_DURATION = 30;

/**
 * @brief The sleep time (in ms) between two executions of the acquiring loop.
 */
static const int ACQUISITION_SLEEP = 100;

 /**
  * @brief Struct to exchange data with recording function.
  *
  * LibSoundIo requires a callback function to save data from its buffer to some
  * other memory area. This callback can handle a user parameter, and we use an
  * instance of this struct as parameter.
  *
  * We use it to pass our output buffer and a variable which allows the control
  * of the input acquisition cycle and the error reporting.
  */
 typedef struct {
 	/**
 	 * @brief The buffer to save samples to.
 	 *
 	 * Instead of using a normal buffer, we use a circular buffer, as advised in
 	 * libSoundIo documentation.
 	 */
 	struct SoundIoRingBuffer *ringBuffer;

 	/**
 	 * @brief A status variable that is used to report errors.
 	 *
 	 * This variable can assume these values:
 	 *  0: no errors;
 	 *  1: ring buffer overflow;
 	 *  2: begin read error;
 	 *  3: end read error.
 	 *
 	 * When status is not 0, the audioRecord loop stops.
 	 */
 	int status;
 } RecordContext;

static struct SoundIoInStream *createStream(struct SoundIoDevice *device);
static void readCallback(struct SoundIoInStream *instream, int frameCountMin,
		int frameCountMax);
static void sleepMs(unsigned int ms);

int audioRecord(AudioContext *context, const char *keepRunning,
		const char *outFileName)
{
	/// An integer used to get errors and report false status when returning
	int err = 0;

	if(!context->device) {
		return 0;
	}

	FILE *out_f = fopen(outFileName, "wb");
	if (!out_f) {
		fprintf(stderr, "Could not open the destination file %s: %s.\n", outFileName, strerror(errno));
		return 0;
	}

	/// The input stream from the device
	struct SoundIoInStream *inStream = createStream(context->device);
	if(!inStream) {
		return 0;
	}

	printf("%dHz %s interleaved\n", inStream->sample_rate,
			soundio_format_string(inStream->format));

	if(err = soundio_instream_open(inStream)) {
		fprintf(stderr, "Could not open input stream: %s.\n",
				soundio_strerror(err));
		return 0;
	}

	int capacity = RING_BUFFER_DURATION * inStream->sample_rate *
			inStream->bytes_per_frame;

	/// A struct to exchange data with the recording callback
	RecordContext rc;
	inStream->userdata = &rc;

	rc.ringBuffer = soundio_ring_buffer_create(context->soundio, capacity);
	if(!rc.ringBuffer) {
		fprintf(stderr, "Could not create the ring buffer.\n");
		return 0;
	}

	rc.status = 0;

	if((err = soundio_instream_start(inStream))) {
		fprintf(stderr, "Could not start input device: %s.\n",
				soundio_strerror(err));
	}

	/* keepRunning must be evaluated as true before recording.
	A 0 value for keepRunning is considered as a logic error and therefore
	it is reported with an assertion in debugging time. */
	assert(*keepRunning);

	/* Check err because if error has already occurred the cycle is skipped and
	cleaning code is immediately run instead.
	TODO: Even though the cycle exits gracefully, sample loss is still a
	problem! As a metter of fact keepRunning is evaluated without checking if
	there are still samples to read. */
	while(*keepRunning && !rc.status && !err) {
		soundio_flush_events(context->soundio);
		sleepMs(ACQUISITION_SLEEP);

	 	int fill_bytes = soundio_ring_buffer_fill_count(rc.ringBuffer);
		char *read_buf = soundio_ring_buffer_read_ptr(rc.ringBuffer);

		size_t amt = fwrite(read_buf, 1, fill_bytes, out_f);
		if ((int)amt != fill_bytes) {
			fprintf(stderr, "Write error: %s.\n", strerror(errno));

			/* Return 0 instead of 1, but don't return immediately because we
			still need to execute cleaning section. */
			err = 1;
			break;
		}

		soundio_ring_buffer_advance_read_ptr(rc.ringBuffer, fill_bytes);
	}

	// Cleaning section
	soundio_ring_buffer_destroy(rc.ringBuffer);
	soundio_instream_destroy(inStream);

	return err == 0;
}

/**
 * @brief Create an input buffer stream.
 *
 * We need to have a buffer with input data, and to create it, libSoundIo needs
 * some parameters, like sample rate and data format.
 *
 * @return The pointer to the instream, or a null pointer in case of error.
 */
static struct SoundIoInStream *createStream(struct SoundIoDevice *device)
{
	if(!device) {
		return 0;
	}

	/// The stream that will be returned.
	struct SoundIoInStream *inStream = soundio_instream_create(device);

	if(!inStream) {
		fprintf(stderr, "Could not create the instream.\n");
		return 0;
	}

	inStream->read_callback = readCallback;

	if(!device->sample_rate_count) {
		fprintf(stderr, "The device doesn't have any sample rate.");
		return 0;
	}

	inStream->sample_rate = SAMPLE_RATES[0];
	for(int const *i = SAMPLE_RATES; *i &&
			!soundio_device_supports_sample_rate(device, *i); i++) {
		inStream->sample_rate = *i;
	}

	if(!inStream->sample_rate) {
		// TODO: Check if that is correct
		/* There isn't any of our preferred frequency, take the nearest.
		Another simpler way would be using the max frequency, but this could
		lead to a failure, because the max rate could be too high and require
		too much memory. This happens, for example, with PulseAudio. */
		inStream->sample_rate = device->sample_rates[0].max;
	}

	/* Our implementation probabily will support only float data, in any case
	checking support formats is a good idea. */
	inStream->format = FORMATS[0];
	for(enum SoundIoFormat const *i = FORMATS; *i != SoundIoFormatInvalid &&
			!soundio_device_supports_format(device, *i); i++) {
		inStream->format = *i;
	}

	if(inStream->format == SoundIoFormatInvalid) {
		fprintf(stderr, "The sound card doesn't support the required input format.");
		return 0;
	}

	return inStream;
}

/**
 * @brief Do something with the acquired data. In this program copies them to a
 * buffer.
 *
 * @note This function must be as fast as possible to be suitable for realtime
 *  purposes. This means it has especially to avoid syscalls and other blocking
 *  functions.
 *
 * See the official documentation of LibSoundIo for further information.
 *
 * @param inStream The input stream to read from
 * @param frameCountMin The minimum frame to read not to lose them
 * @param frameCountMax The maximum frame that the function can read
 */
static void readCallback(struct SoundIoInStream *inStream, int frameCountMin,
		int frameCountMax)
{
	RecordContext *rc = inStream->userdata;

	/* This should never happen, however if the status is not clean, continuing
	the execution is not a good idea, so return immediately. */
	if(rc->status) {
		return;
	}

	// These variables are needed by libSoundIo
	struct SoundIoChannelArea *areas;
	int err;
	char *writePtr = soundio_ring_buffer_write_ptr(rc->ringBuffer);

	int freeBytes = soundio_ring_buffer_free_count(rc->ringBuffer);
	int freeCount = freeBytes / inStream->bytes_per_frame;
	if(freeCount < frameCountMin) {
		fprintf(stderr, "Ring buffer overflow\n");

		rc->status = 1;
		return;
	}
	int writeFrames = freeCount < frameCountMax ? freeCount : frameCountMax;

	/* An assertion here isn't really needed, because the code still works, but
	since we've already checked for overflows, the assertion should do something
	only when frameCountMin and frameCountMax are both 0, which doesn't make
	sense and deserves to be noticed during programming time. */
	assert(writeFrames);

	for(int frameCount = writeFrames, framesLeft = writeFrames;
			framesLeft > 0; framesLeft -= frameCount) {
		if(err = soundio_instream_begin_read(inStream, &areas, &frameCount)) {
			fprintf(stderr, "Begin read error: %s", soundio_strerror(err));

			rc->status = 2;
			return;
		}

		/* Note: frameCount is used as output parameter in previous function.
		Therefore this is the only way to exit the cycle in this case. */
		if(!frameCount) {
			break;
		}

		if(!areas) {
			/* Due to an overflow there is a hole. Fill the ring buffer with
			silence for the size of the hole.
			Note: a silence is ok for registration, but for audio detection
			could not be very useful. A flag to report silence and so clear the
			state of the played note would be better. */
			memset(writePtr, 0, frameCount * inStream->bytes_per_frame);
		} else {
			for(int frame = 0; frame < frameCount; frame++) {
				for(int ch = 0; ch < inStream->layout.channel_count; ch++) {
					memcpy(writePtr, areas[ch].ptr, inStream->bytes_per_sample);
					areas[ch].ptr += areas[ch].step;
					writePtr += inStream->bytes_per_sample;
				}
			}
		}

		if(err = soundio_instream_end_read(inStream)) {
			fprintf(stderr, "End read error: %s", soundio_strerror(err));

			rc->status = 3;
			return;
		}
	}

	int advanceBytes = writeFrames * inStream->bytes_per_frame;
	soundio_ring_buffer_advance_write_ptr(rc->ringBuffer, advanceBytes);
}

/**
 * @brief A sleep function with milliseconds precision.
 * @author Bernardo Ramos (http://stackoverflow.com/users/4626775/bernardo-ramos)
 * @link http://stackoverflow.com/a/28827188
 * @copyright Creative Commons Attribution-ShareAlike 3.0 Unported
 *
 * This function has been taken from Stack Overflow.
 *
 * @param ms The milliseconds to sleep
 */
inline static void sleepMs(unsigned int ms) {
#ifdef WIN32
	Sleep(ms);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&ts, NULL);
#elif defined(__linux__)
	usleep(ms * 1000);
#else
#	error "Unsupported platform"
#endif
}

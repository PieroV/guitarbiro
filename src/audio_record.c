/**
 * @file audio_record.c
 * @brief Manages the acquisition of samples from the audio card.
 *
 * @link http://libsound.io/doc-1.1.0/sio_list_devices_8c-example.html
 */

#include "audio.h"

#include "detect.h"

// printf, scanf, fprintf
#include <stdio.h>
// malloc, free
#include <stdlib.h>
// memset, memcpy
#include <string.h>
// assert
#include <assert.h>

// Endianness test done by SoundIo
#include <soundio/endian.h>

#ifdef WIN32
	// Sleep
#	include <windows.h>
#elif defined(__linux__)
	// usleep (on Linux)
#	include <unistd.h>
#endif

#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
	// nanosleep
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
 * Probably the frequency algorithm needs float data, and since we cannot make
 * a template in C, we will accept only this format of data, in native
 * endianness.
 */
static const enum SoundIoFormat FORMATS[] = {
#ifdef SOUNDIO_OS_LITTLE_ENDIAN
	SoundIoFormatFloat32LE,
#endif
#ifdef SOUNDIO_OS_BIG_ENDIAN
	SoundIoFormatFloat32BE,
#endif
	SoundIoFormatInvalid,
};

/**
 * @brief How much space allocate for the temporary buffer, measured in seconds.
 */
static const int RING_BUFFER_DURATION = 30;

/**
 * @brief The sleep time (in ms) between two executions of the acquiring loop.
 */
static const int ACQUISITION_SLEEP = 20;

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

int audioRecord(AudioContext *context, const char *keepRunning)
{
	/**
	 * An integer used to get errors and report false status when returning.
	 *
	 * err != 0 will make skip all further blocks, except for the cleaning one.
	 * In most cases, err is used instead of returing directly in order to make
	 * sure that all resources are freed correctly.
	 */
	int err = 0;
	/// A struct to exchange data with the recording callback
	RecordContext rc;
	/// The context for detection functions
	DetectContext *detection = 0;

	if(!context->device) {
		return 0;
	}

	/// The input stream from the device
	struct SoundIoInStream *inStream = createStream(context->device);
	if(!inStream) {
		return 0;
	}

	inStream->userdata = &rc;
	rc.status = 0;

	if(err = soundio_instream_open(inStream)) {
		fprintf(stderr, "Could not open input stream: %s.\n",
				soundio_strerror(err));
	}

	if(!err) {
		int capacity = RING_BUFFER_DURATION * inStream->sample_rate *
				inStream->bytes_per_frame;
		rc.ringBuffer = soundio_ring_buffer_create(context->soundio, capacity);

		if(!rc.ringBuffer) {
			fprintf(stderr, "Could not create the ring buffer.\n");
			err = 1;
		}
	}

	if(!err) {
		detection = detectInit(inStream->sample_rate);
		err = detection == 0;
	}

	if(!err && (err = soundio_instream_start(inStream))) {
		fprintf(stderr, "Could not start input device: %s.\n",
				soundio_strerror(err));
	}

	/* keepRunning must be evaluated as true before recording.
	A 0 value for keepRunning is considered as a logic error and therefore
	it is reported with an assertion in debugging time. */
	assert(*keepRunning);

	while(*keepRunning && !rc.status && !err) {
		soundio_flush_events(context->soundio);
		sleepMs(ACQUISITION_SLEEP);

		err = detectAnalyze(detection, rc.ringBuffer);
	}

	/* Pause the stream so that readingCallback won't be called anymore and
	won't cause troubles, now that the ring buffer will be destroyed. */
	soundio_instream_pause(inStream, 1);
	soundio_instream_destroy(inStream);

	// Cleaning section
	if(rc.ringBuffer) {
		if(!err) {
			// Be sure to analyze last data, too
			err = detectAnalyze(detection, rc.ringBuffer);
		}

		// SoundIo documentation says nothing about cleaning a null ringBuffer.
		soundio_ring_buffer_destroy(rc.ringBuffer);
	}

	// A null detection isn't a problem, so leave the check to detectFree
	detectFree(detection);

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

	inStream->layout = *soundio_channel_layout_get_default(1);

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
		inStream->sample_rate = soundio_device_nearest_sample_rate(device,
				SAMPLE_RATES[0]);
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

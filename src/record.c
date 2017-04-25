/**
 * @file record.c
 * @brief Manages the acquisition of samples from the audio card.
 *
 * @link http://libsound.io/doc-1.1.0/sio_list_devices_8c-example.html
 */

#include "record.h"

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

/// sleep
#include <unistd.h>

#include <soundio/soundio.h>

/**
 * @brief The SoundIo struct.
 *
 * We need to share the SoundIo struct between all the functions that are part
 * of the recording interface (init, record), but we don't want to give directly
 * the details of the SoundIo struct externally, so we make it a static variable
 * of this file.
 */
static struct SoundIo *gSoundio = 0;

/**
 * TODO: find another way to do this
 */
static struct SoundIoDevice *gInput = 0;

/* == Initialization == */
static enum SoundIoBackend chooseBackend();
static struct SoundIoDevice *getInput();

/**
 * @todo Add automatic destruction of SoundIo with atexit?
 */
int audioInit()
{
	if(gSoundio) {
		return -1;
	}

	gSoundio = soundio_create();
	if(!gSoundio) {
		fprintf(stderr, "Could not allocate the SoundIo structure.");
		return 1;
	}

	enum SoundIoBackend backend = chooseBackend();
	/* We assume backend won't ever be SoundIoBackendNone, as the do-while loop
	would not end in case, but in future if this isn't guaranteed anymore, we
	should check. */
	int err = soundio_connect_backend(gSoundio, backend);
	if(err) {
		fprintf(stderr, "Error: %s\n", soundio_strerror(err));
		return 1;
	}

	// getInput has already printed the error to the user
	// TODO: Find another way to do this!
	if(!(gInput = getInput())) {
		return 2;
	}

	return 0;
}

/**
 * @brief Asks the user for the backend he or she wants to use.
 * @todo Check for available backends.
 */
static enum SoundIoBackend chooseBackend()
{
	enum SoundIoBackend backend = SoundIoBackendNone;

	printf("Please enter the backend you want to use:\n");
	do {
		printf(" a) ALSA\n c) CoreAudio\n j) JACK\n p) PulseAudio \n"
				" w) Wsapi\n d) dummy\n");

		char choice;
		scanf("%c", &choice);

		switch(choice) {
			case 'a':
				backend = SoundIoBackendAlsa;
				break;

			case 'c':
				backend = SoundIoBackendCoreAudio;
				break;

			case 'j':
				backend = SoundIoBackendJack;
				break;

			case 'p':
				backend = SoundIoBackendPulseAudio;
				break;

			case 'w':
				backend = SoundIoBackendWasapi;
				break;

			case 'd':
				backend = SoundIoBackendDummy;
				break;

			default:
				printf("Invalid choice! Please enter it again.\n\n");
		}
	} while(backend == SoundIoBackendNone);

	return backend;
}

/**
 * @brief Asks the user which input is the guitar plugged in.
 * @return The device to use to record, or the null pointer in case of errror.
 * @note Remember to unref the device pointer when you have finished to use it.
 */
struct SoundIoDevice *getInput()
{
	struct SoundIoDevice *device = 0;

	soundio_flush_events(gSoundio);
	int inputCount = soundio_input_device_count(gSoundio);
	int defaultInput = soundio_default_input_device_index(gSoundio);

	if(!inputCount) {
		fprintf(stderr, "Error: no input devices found.");
		return 0;
	}

	printf("Please choose an input device:\n");

	for(int i = 0; i < inputCount; i++) {
		device = soundio_get_input_device(gSoundio, i);

		const char *defaultStr = i == defaultInput ? " (default)" : "";
	    const char *rawStr = device->is_raw ? " (raw)" : "";
	    printf(" %d) %s%s%s\n", i + 1, device->name, defaultStr, rawStr);

		soundio_device_unref(device);
	}

	int chosen;
	scanf("%d", &chosen);
	while(chosen < 1 || chosen > inputCount) {
		printf("Invalid choice! Please choose the device again: ");
		scanf("%d", &chosen);
	}

	/* Since we haven't flushed events, we assume the list hasn't changed
	internally. Otherwise we should have saved pointers to the pointers of the
	devices, and then unref all but the one the user has chosen. */
	device = soundio_get_input_device(gSoundio, --chosen);

	if(device->probe_error) {
		printf("Unable to probe device: %s.\n",
				soundio_strerror(device->probe_error));
		device = 0;
	}

	return device;
}

/* == Recording == */
/**
 *
 */
struct RecordContext {
    struct SoundIoRingBuffer *ring_buffer;
};

static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

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

static struct SoundIoInStream *createStream();
static void readCallback(struct SoundIoInStream *instream, int frameCountMin, int frameCountMax);

int audioRecord(const char *outFileName)
{
	// An integer used to get errors
	int err = 0;

	if(!gInput) {
		return 0;
	}

	FILE *out_f = fopen(outFileName, "wb");
	if (!out_f) {
		fprintf(stderr, "Could not open the destination file %s: %s.\n", outFileName, strerror(errno));
		return 0;
	}

	struct SoundIoInStream *instream = createStream();
	if(!instream) {
		return 0;
	}

	printf("%dHz %s interleaved\n", instream->sample_rate,
			soundio_format_string(instream->format));

	if(err = soundio_instream_open(instream)) {
		fprintf(stderr, "Could not open input stream: %s.\n",
				soundio_strerror(err));
		return 0;
	}

	const int ring_buffer_duration_seconds = 30;
	int capacity = ring_buffer_duration_seconds * instream->sample_rate * instream->bytes_per_frame;

	/// A struct to exchange data with the recording callback
	struct RecordContext rc;
	rc.ring_buffer = soundio_ring_buffer_create(gSoundio, capacity);
	if(!rc.ring_buffer) {
		fprintf(stderr, "Could not create the ring buffer.\n");
		return 0;
	}
	instream->userdata = &rc;

	if((err = soundio_instream_start(instream))) {
		fprintf(stderr, "Could not start input device: %s.\n", soundio_strerror(err));
		return 0;
	}

	/* TODO: Add a strategy to finish recording!
	This is just to test if libSoundIo works and it requires to stop it using
	CTRL+C, and therefore lose up to 1 second of recorded audio data. */
	while(1) {
		soundio_flush_events(gSoundio);
		sleep(1);

	 	int fill_bytes = soundio_ring_buffer_fill_count(rc.ring_buffer);
		char *read_buf = soundio_ring_buffer_read_ptr(rc.ring_buffer);

		size_t amt = fwrite(read_buf, 1, fill_bytes, out_f);
		if ((int)amt != fill_bytes) {
			fprintf(stderr, "Write error: %s.\n", strerror(errno));
			return 0;
		}

		soundio_ring_buffer_advance_read_ptr(rc.ring_buffer, fill_bytes);
	}

	soundio_instream_destroy(instream);
	soundio_device_unref(gInput);
	soundio_destroy(gSoundio);

	return 1;
}

/**
 * @brief Create an input buffer stream.
 *
 * We need to have a buffer with input data, and to create it, libSoundIo needs
 * some parameters, like sample rate and data format.
 *
 * @return The pointer to the instream, or a null pointer in case of error.
 */
static struct SoundIoInStream *createStream()
{
	/* This is a static function, and assume that important things have already
	been checked. */
	assert(gInput);

	/// The stream that will be returned.
	struct SoundIoInStream *instream = soundio_instream_create(gInput);

	if (!instream) {
		fprintf(stderr, "Could not create the instream.\n");
		return 0;
	}

	instream->read_callback = readCallback;

	if(!gInput->sample_rate_count) {
		fprintf(stderr, "The device doesn't have any sample rate.");
		return 0;
	}

	instream->sample_rate = SAMPLE_RATES[0];
	for(int *i = SAMPLE_RATES; *i &&
			!soundio_device_supports_sample_rate(gInput, *i); i++) {
		instream->sample_rate = *i;
	}

	if(!instream->sample_rate) {
		// TODO: Check if that is correct
		/* There isn't any of our preferred frequency, take the nearest.
		Another simpler way would be using the max frequency, but this could
		lead to a failure, because the max rate could be too high and require
		too much memory. This happens, for example, with PulseAudio. */
		instream->sample_rate = gInput->sample_rates[0].max;
	}

	/* Our implementation probabily will support only float data, in any case
	checking support formats is a good idea. */
	instream->format = FORMATS[0];
	for(enum SoundIoFormat *i = FORMATS; *i != SoundIoFormatInvalid &&
			!soundio_device_supports_format(gInput, *i); i++) {
		instream->format = *i;
	}

	if(instream->format == SoundIoFormatInvalid) {
		fprintf(stderr, "The sound card doesn't support the required input format.");
		return 0;
	}

	return instream;
}

/**
 * @brief Do something with the acquired data. In this program copies them to a
 * buffer.
 *
 * @note This function must be as fast as possible to be suitable for realtime
 * purposes. This means it has especially to avoid syscalls and other blocking
 * functions.
 *
 * See the official documentation of LibSoundIo for further information.
 *
 * @param instream The input stream to read from
 * @param frameCountMin The minimum frame to read not to lose them
 * @param frameCountMax The maximum frame that the function can read
 */
static void readCallback(struct SoundIoInStream *instream, int frameCountMin,
		int frameCountMax) {
    struct RecordContext *rc = instream->userdata;
    struct SoundIoChannelArea *areas;
    int err;

    char *writePtr = soundio_ring_buffer_write_ptr(rc->ring_buffer);
    int freeBytes = soundio_ring_buffer_free_count(rc->ring_buffer);

    int freeCount = freeBytes / instream->bytes_per_frame;
    if (freeCount < frameCountMin) {
        fprintf(stderr, "ring buffer overflow\n");
        exit(1);
    }

    int writeFrames = min_int(freeCount, frameCountMax);

	/* An assertion here isn't really needed, because the code still works, but
	since we've already checked for overflows, the assertion should do something
	only when frameCountMin and frameCountMax are both 0, which doesn't make
	sense and deserves to be noticed during programming time. */
	assert(writeFrames);

    for(int frameCount = writeFrames, framesLeft = writeFrames;
		 		framesLeft > 0; framesLeft -= frameCount) {
        if(err = soundio_instream_begin_read(instream, &areas, &frameCount)) {
			// TODO: Set a flag to stop reading in audioRecord instead of exiting
            fprintf(stderr, "begin read error: %s", soundio_strerror(err));
            exit(1);
        }

		// Note: frameCount is used as output parameter in previous function
        if (!frameCount) {
            break;
		}

        if (!areas) {
            /* Due to an overflow there is a hole. Fill the ring buffer with
            silence for the size of the hole.
			Note: a silence is ok for registration, but for audio detection
			could not be very useful. A flag to report silence and so clear the
			state of the played note would be better. */
            memset(writePtr, 0, frameCount * instream->bytes_per_frame);
        } else {
            for(int frame = 0; frame < frameCount; frame++) {
                for(int ch = 0; ch < instream->layout.channel_count; ch++) {
                    memcpy(writePtr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    writePtr += instream->bytes_per_sample;
                }
            }
        }

		// TODO: Set a flag to stop reading in audioRecord instead of exiting
        if (err = soundio_instream_end_read(instream)) {
            fprintf(stderr, "end read error: %s", soundio_strerror(err));
            exit(1);
        }
    }

    int advanceBytes = writeFrames * instream->bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(rc->ring_buffer, advanceBytes);
}

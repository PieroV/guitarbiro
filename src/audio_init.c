/**
 * @file audio_init.c
 * @brief Manages the initialization of audio system.
 */

#include "audio.h"

/// printf, scanf, fprintf
#include <stdio.h>

/**
 * @brief Asks the user for the backend he or she wants to use.
 * @todo Check for available backends.
 */
static enum SoundIoBackend chooseBackend();

/**
 * @brief Asks the user which input is the guitar plugged in.
 * @return The device to use to record, or the null pointer in case of errror.
 * @note Remember to unref the device pointer when you have finished to use it.
 */
static struct SoundIoDevice *getInputDevice(struct SoundIo *soundio);

int audioInit(AudioContext *context)
{
	/* The context could be dirty, so be sure to clean as first thing, to avoid
	the unref of memory areas that haven't actually been allocated.
	On the contrary, we won't clean soundio, because we will change it
	immediately. */
	context->device = 0;

	context->soundio = soundio_create();
	if(!context->soundio) {
		fprintf(stderr, "Could not allocate the SoundIo structure.");
		return 1;
	}

	enum SoundIoBackend backend = chooseBackend();
	/* We assume backend won't ever be SoundIoBackendNone, as the do-while loop
	would not end in case, but in future if this isn't guaranteed anymore, we
	should check. */
	int err = soundio_connect_backend(context->soundio, backend);
	if(err) {
		fprintf(stderr, "Error: %s\n", soundio_strerror(err));
		return 1;
	}

	if(!(context->device = getInputDevice(context->soundio))) {
		// getInputDevice has already printed the error to the user
		return 2;
	}

	return 0;
}

static enum SoundIoBackend chooseBackend()
{
	/// The enum that will be returned
	enum SoundIoBackend backend = SoundIoBackendNone;

	// TODO: Print only backends that are actually available
	printf("Please enter the backend you want to use:\n");
	do {
		printf(" a) ALSA\n c) CoreAudio\n j) JACK\n p) PulseAudio \n"
				" w) Wsapi\n d) dummy\n");

		/// A temporary variable that stores the backend choice
		char choice;
		// TODO: if CLI interface will be kept, clean spaces
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

struct SoundIoDevice *getInputDevice(struct SoundIo *soundio)
{
	/// The struct pointer that will be returned
	struct SoundIoDevice *device = 0;

	// An event flush is always required to have the updated list of devices.
	soundio_flush_events(soundio);
	/// The number of input devices
	int inputCount = soundio_input_device_count(soundio);
	/// The index of the default input device
	int defaultInput = soundio_default_input_device_index(soundio);

	if(!inputCount) {
		fprintf(stderr, "Error: no input devices found.");
		return 0;
	}

	printf("Please choose an input device:\n");

	for(int i = 0; i < inputCount; i++) {
		device = soundio_get_input_device(soundio, i);

		const char *defaultStr = i == defaultInput ? " (default)" : "";
	    const char *rawStr = device->is_raw ? " (raw)" : "";
	    printf(" %d) %s%s%s\n", i + 1, device->name, defaultStr, rawStr);

		soundio_device_unref(device);
	}

	/// A temporary integer that stores the device choice
	int chosen;

	scanf("%d", &chosen);
	while(chosen < 1 || chosen > inputCount) {
		printf("Invalid choice! Please choose the device again: ");
		scanf("%d", &chosen);
	}

	int tmp;
	while((tmp = getchar()) != '\n' && tmp != EOF);

	/* Since we haven't flushed events, we assume the list hasn't changed
	internally. Otherwise we should have saved pointers to the pointers of the
	devices, and then unref all but the one the user has chosen. */
	device = soundio_get_input_device(soundio, --chosen);

	if(device->probe_error) {
		printf("Unable to probe device: %s.\n",
				soundio_strerror(device->probe_error));
		soundio_device_unref(device);
		device = 0;
	}

	return device;
}

void audioClose(AudioContext *context)
{
	if(context->device) {
		soundio_device_unref(context->device);
		context->device = 0;
	}

	if(context->soundio) {
		soundio_destroy(context->soundio);
		context->soundio = 0;
	}
}

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

/* == Initialization section == */
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
	if(!getInput()) {
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
	soundio_flush_events(gSoundio);
	int inputCount = soundio_input_device_count(gSoundio);
	int defaultInput = soundio_default_input_device_index(gSoundio);

	if(!inputCount) {
		fprintf(stderr, "Error: no input devices found.");
		return 0;
	}

	printf("Please choose an input device:\n");

	for(int i = 0; i < inputCount; i++) {
		struct SoundIoDevice *device = soundio_get_input_device(gSoundio, i);

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
	return soundio_get_input_device(gSoundio, --chosen);
}

void audioRecord()
{
}

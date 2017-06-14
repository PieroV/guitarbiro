/**
 * @file audio_init.c
 * @brief Manages the initialization of audio system.
 */

#include "audio.h"

/// printf, scanf, fprintf
#include <stdio.h>
/// malloc, free
#include <stdlib.h>
/// strlen
#include <string.h>

/**
 * @brief Open and return a device, given its index
 *
 * @param soundio The SoundIo instance
 * @param index The index of the device in the internal SoundIo list
 * @return The pointer to the device struct, or NULL in case of error
 */
static int setupInputDevice(struct SoundIoDevice *device);

AudioContext *audioInit()
{
	AudioContext *context = malloc(sizeof(AudioContext));

	/* The context could be dirty, so be sure to clean as first thing, to avoid
	the unref of memory areas that haven't actually been allocated.
	On the contrary, we won't clean soundio, because we will change it
	immediately. */
	context->device = 0;

	context->soundio = soundio_create();
	if(!context->soundio) {
		fprintf(stderr, "Could not allocate the SoundIo structure.");
		free(context);
		return 0;
	}

	int err = soundio_connect(context->soundio);
	if(err) {
		fprintf(stderr, "Error: %s\n", soundio_strerror(err));
		free(context);
		return 0;
	}

	// We need device information in order to open the default device
	soundio_flush_events(context->soundio);
	int defaultDevice = soundio_default_input_device_index(context->soundio);
	context->device = soundio_get_input_device(context->soundio, defaultDevice);

	if(!setupInputDevice(context->device)) {
		soundio_device_unref(context->device);
		context->device = 0;
	}

	/* Don't block the program in case of failure while opening the default
	device, so that it can be changed using the settings dialog. */

	return context;
}

char const **audioGetBackends(AudioContext *context, int *count, int *current)
{
	if(current) {
		// In any case set this error condition
		*current = -1;
	}

	if(!context || !context->soundio) {
		*count = 0;
		return 0;
	}

	*count = soundio_backend_count(context->soundio);
	char const **ret = (char const **) malloc(sizeof(const char *) * *count);

	for(int i = 0; i < *count; i++) {
		enum SoundIoBackend backend = soundio_get_backend(context->soundio, i);
		ret[i] = soundio_backend_name(backend);

		if(*current && backend == context->soundio->current_backend) {
			*current = i;
		}
	}

	return ret;
}

const char *audioGetCurrentBackend(AudioContext *context)
{
	if(!context || !context->soundio) {
		return 0;
	}

	return soundio_backend_name(context->soundio->current_backend);
}

char **audioGetDevices(AudioContext *context, int *count, int *current)
{
	char **ret;
	struct SoundIoDevice *device;

	if(current) {
		// In any case set this error condition
		*current = -1;
	}

	if(!context || !context->soundio) {
		*count = 0;
		return 0;
	}

	// An event flush is always required to have the updated list of devices.
	soundio_flush_events(context->soundio);

	if(current) {
		*current = soundio_default_input_device_index(context->soundio);
	}

	*count = soundio_input_device_count(context->soundio);
	ret = (char **) malloc(sizeof(const char *) * *count);

	for(int i = 0; i < *count; i++) {
		device = soundio_get_input_device(context->soundio, i);

		// Sadly strdup isn't standard...
		ret[i] = malloc(strlen(device->name) + 1);
		strcpy(ret[i], device->name);

		// If the in-use device is not the default one...
		if(current && context->device &&
				soundio_device_equal(context->device, device)) {
			*current = i;
		}

		soundio_device_unref(device);
	}

	return ret;
}

char **audioGetBackendDevices(const char *backend, int *count,
		int *defaultDevice)
{
	/**
	 * @brief A temporary AudioContext instance
	 *
	 * We use a temporary AudioContext instead of creating a helper because it
	 * would in any case require an audiocontext to set the "current" paramter,
	 * or we should retierate on the other version of audioGetDevices to set it.
	 */
	AudioContext tmpContext;
	char **ret;

	tmpContext.soundio = soundio_create();
	tmpContext.device = 0;

	if(audioSetBackend(&tmpContext, backend)) {
		*count = 0;
		ret = 0;
	} else {
		ret = audioGetDevices(&tmpContext, count, defaultDevice);
	}

	/* Destroy soundio manually, as audioClose would try to free the temporary
	context that actually is on stack. */
	soundio_disconnect(tmpContext.soundio);
	soundio_destroy(tmpContext.soundio);

	return ret;
}

int audioSetBackend(AudioContext *context, const char *backend)
{
	/// The backend that soundio will connect to
	enum SoundIoBackend backendId = SoundIoBackendNone;
	/// The number of available backends
	int count;
	/// The error code of soundio_connect_backend
	int err;

	count = soundio_backend_count(context->soundio);
	if(!count || !backend) {
		return 1;
	}

	/* audioGetBackends could have been used, but since we need the backend from
	the enum we would have had to reiterate again all the backends, so iterate
	once here */
	for(int i = 0; i < count && backendId == SoundIoBackendNone; i++) {
		enum SoundIoBackend tmp = soundio_get_backend(context->soundio, i);
		const char *name = soundio_backend_name(tmp);
		if(!strcmp(name, backend)) {
			backendId = tmp;
		}
	}

	if(backendId == SoundIoBackendNone) {
		return 1;
	}

	if(context->soundio->current_backend != SoundIoBackendNone) {
		soundio_disconnect(context->soundio);
	}

	err = soundio_connect_backend(context->soundio, backendId);
	return err != 0;
}

const char *audioGetCurrentDevice(AudioContext *context)
{
	return context->device->name;
}

int audioSetDevice(AudioContext *context, const char *name)
{
	if(!context || !context->soundio || !name) {
		return 0;
	}

	if(context->device && !strcmp(context->device->name, name)) {
		return 1;
	}

	if(context->device) {
		soundio_device_unref(context->device);
		context->device = 0;
	}

	soundio_flush_events(context->soundio);

	int count = soundio_input_device_count(context->soundio);
	for(int i = 0; i < count && !context->device; i++) {
		context->device = soundio_get_input_device(context->soundio, i);
		if(strcmp(context->device->name, name)) {
			soundio_device_unref(context->device);
			context->device = 0;
		}
	}

	if(context->device && !setupInputDevice(context->device)) {
		soundio_device_unref(context->device);
		context->device = 0;
	}

	return context->device != 0;
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

	free(context);
}

int setupInputDevice(struct SoundIoDevice *device)
{
	if(device->probe_error) {
		fprintf(stderr, "Unable to probe device: %s.\n",
				soundio_strerror(device->probe_error));
		return 0;
	}

	struct SoundIoChannelLayout const *mono =
			soundio_channel_layout_get_default(1);
	if(!soundio_device_supports_layout(device, mono)) {
		fprintf(stderr, "The selected device does not support mono layout.\n");
		return 0;
	}

	return 1;
}

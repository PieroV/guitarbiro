/**
 * @file audio.h
 * @brief Manages the acquisition of samples from the audio card.
 */

#ifndef __AUDIO_H
#define __AUDIO_H

// size_t
#include <stddef.h>

#include <soundio/soundio.h>

/**
 * @brief A struct which is used to pass data between audio functions.
 *
 * Since in the approach we decided to use audio is managed through different
 * functions that are in different files, we need to pass some data between
 * them, such as the libSoundIo instance we have created and the device that we
 * use to acquire data from.
 *
 * The user should trat instances of this struct like they were constant, i.e.
 * she or he should not change the content of the struct variables.
 */
typedef struct {
	/**
	 * @brief The libSoundIo instance.
	 */
	struct SoundIo *soundio;

	/**
	 * @brief The device to acquire data from.
	 */
	struct SoundIoDevice *device;
} AudioContext;

/**
 * @brief Initializes libSoundIo.
 *
 * Initializes libSoundIo and connects it to the default backend.
 *
 * @return An instance of AudioContext struct, or NULL in case of error
 */
extern AudioContext *audioInit();

/**
 * @brief Closes all handlers to devices and destroys the libSoundIo instance.
 *
 * @note The function doesn't call free on context (to avoid problems if it is
 * not in heap).
 *
 * @param context The AudioContext instance
 */
extern void audioClose(AudioContext *context);

/**
 * @brief Get the list of audio backends.
 *
 * @param context The AudioContext instance
 * @param count An output parameter that will contain the number of available
 *  backends. Note that it must be a valid pointer
 * @param current An output parameter that will contain the index of the current
 *  backend. If null it won't be used
 * @return The list of backend names. The caller will have to free it
 */
extern char const **audioGetBackends(AudioContext *context, int *count,
		int *current);

/**
 * @brief Get the name of the backend that is currently being used.
 *
 * @param context The AudioContext instance
 */
extern const char *audioGetCurrentBackend(AudioContext *context);

/**
 * @brief Get the list of input devices that the connected backend can use.
 *
 * @param context The AudioContext instance
 * @param count An output parameter that will contain the number of available
 *  devices. Note that it must be a valid pointer
 * @param current An output parameter that will contain the index of the current
 *  or the default device. If null it won't be used
 * @return The list of device names. The caller will have to free all the items
 *  and then the array (not only the array!)
 */
extern char **audioGetDevices(AudioContext *context, int *count, int *current);

/**
 * @brief Get the list of input devices that an audio backend can use.
 *
 * This function is very similar to audioGetDevices, but instead of using the
 * current audio context and the current backend, it creates a new one, thus
 * allowing to get temporary list of devices, without having to worry about
 * stopping the recording, closing the current context, etc etc...
 *
 * @param backend The name of the backend to get the device of
 * @param count An output parameter that will contain the number of available
 *  devices. Note that it must be a valid pointer
 * @param defaultDevice An output parameter that will contain the default input
 *  device for that backend. If NULL it will be ignored
 * @return The list of device names. The caller will have to free all the items
 *  and then the array (not only the array!)
 */
 extern char **audioGetBackendDevices(const char *backend, int *count,
		int *defaultDevice);

/**
 * @brief Get the name of the device that is currently being used.
 *
 * @param context The AudioContext instance
 */
extern const char *audioGetCurrentDevice(AudioContext *context);

/**
 * @brief Connect soundio to (another) backend.
 *
 * @param context The AudioContext instance
 * @param backend The backend name
 * @return A status code:
 *  * 0 in case of success
 *  * 1 in case that the chosen backend is not available, but the current
 *    context remains valid
 *  * 2 if the connection failed, therefore the current context isn't valid
 *    anymore
 */
extern int audioSetBackend(AudioContext *context, const char *backend);

/**
 * @brief Change input device.
 *
 * @note This function won't check if audioRecord is running, the caller has to
 *  check it before calling this function. An undefined behaviour might happen
 *  otherwise.
 *
 * @param context The AudioContext instance
 * @param name The name of the device to use
 * @return 1 in case of success, 0 in case of error
 */
extern int audioSetDevice(AudioContext *context, const char *name);

/**
 * @brief Start recording.
 *
 * For this test code, it will acquire some samples from the selected device
 * and it will save them to the file that is passed as argument.
 *
 * Since we don't to provide a time or sample number based condition to stop the
 * recording, we allow it to be controlled using an external flag.
 * Due to its constant nature, it must not be 0 before the recording starts.
 * Since this would be a logic error, it is controlled using an assertion.
 * The flag has been chosen to be const to avoid any problem with
 * synchronization.
 *
 * @param context The AudioContext instance
 * @param keepRunning A flag that allows to control the audio recording
 * @return The status (boolean)
 */
extern int audioRecord(AudioContext *context, const char *keepRunning);

#endif /* __AUDIO_H */

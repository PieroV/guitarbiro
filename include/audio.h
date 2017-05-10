/**
 * @file record.h
 * @brief Manages the acquisition of samples from the audio card.
 */

#ifndef __AUDIO_H
#define __AUDIO_H

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
 * Initializes libSoundIo, then prompt the user for the backend and the
 * interface to use using the command line.
 *
 * The context struct must be a valid area of memory. The caller of this
 * function will remain its owner and will have to call free after having called
 * audioClose, eventually.
 *
 * @note If this function fails (non 0 returned code), calling audioClose will
 *  remain necessary.
 *
 * @param context An instance of the AudioContext struct, that has to be
 *  allocated by the caller, even though she/he should not modify it.
 * @return 0 in case of success, another integer in case of error:
 *  1 - an error occurred while initializating libSoundIo
 *  2 - an error occurred while choosing/opening the device
 */
extern int audioInit(AudioContext *context);

/**
 * @brief Closes all handlers to devices and destroys the libSoundIo instance.
 *
 * @note The function doesn't call free on context (to avoid problems if it is
 * not in heap).
 *
 * @param context The AudioContext instance. Note that if it has been allocated
 *  on heap, e.g. through malloc, the caller will have to free it.
 */
extern void audioClose(AudioContext *context);

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
 * @param outFileName The name of the file to save audio to
 * @param keepRunning A flag that allows to control the audio recording
 * @return The status (boolean)
 */
extern int audioRecord(AudioContext *context, const char *keepRunning,
		const char *outFileName);

#endif /* __AUDIO_H */

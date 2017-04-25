/**
 * @file record.h
 * @brief Manages the acquisition of samples from the audio card.
 */

#ifndef __RECORD_H
#define __RECORD_H

/**
 * @brief Initializes libSoundIo.
 * @return 0 in case of success, another integer in case of error.
 *
 * Initializes libSoundIo, then prompt the user for the backend and the
 * interface to use using the command line.
 *
 * Return codes are:
 *  0 - everything succeeded
 *  1 - an error occurred while initializating libSoundIo
 *  2 - an error occurred while choosing/opening the device
 *  -1 - the function had already been called
 */
extern int audioInit();

/**
 * @brief Start recording.
 *
 * For this test code, it will acquire some samples from the selected device
 * and it will save them to the file that is passed as argument.
 *
 * @param outFileName The name of the file to save audio to
 * @return The status (boolean)
 */
extern int audioRecord(const char *outFileName);

#endif /* __RECORD_H */

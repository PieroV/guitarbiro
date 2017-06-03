/**
 * @file detect.h
 * @brief Detect which note has been played in a sequency of audio samples.
 */

/// SoundIoRingBuffer
#include <soundio/soundio.h>

/**
 * @brief The lowest note to detect.
 *
 * The format of this macro must be a string for the note name and a semitone_t
 * with the octave, separated by a comma.
 *
 * E1 is an octave below the lowest note on a standard tuned guitar.
 */
#define DETECT_LOWEST "E", 1

/**
 * @brief The highest note to detect.
 *
 * The format of this macro must be a string for the note name and a semitone_t
 * with the octave, separated by a comma.
 *
 * E7 is an octave above the highest note that a 24-frets standard tuned guitar
 * can make.
 */
#define DETECT_HIGHEST "E", 7

/**
 * @brief A struct to share data between the detection functions.
 */
typedef struct _DetectContext DetectContext;

/**
 * @brief Initialize a DetectContext.
 *
 * @param rate The sampling rate of frequencies
 * @return An instance of DetectContext or 0 in case of error
 */
extern DetectContext *detectInit(unsigned int rate);

/**
 * @brief Free a DetectContext.
 * @note If context is null, the function will safely return without doing
 *  anything.
 *
 * @param context A valid DetectContext or null
 */
extern void detectFree(DetectContext *context);

/**
 * @brief Analyze some audio samples and outputs the played note to the user.
 *
 * @param context A valid DetectContext instance
 * @param buffer The audio buffer
 * @return An error code, 0 if no errors occurred
 */
extern int detectAnalyze(DetectContext *context,
		struct SoundIoRingBuffer *buffer);

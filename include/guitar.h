/**
 * @file guitar.h
 * @brief Utilities for operations on musical notes and on guitar "structure".
 *
 * The program needs a way to do operations on musical notes, such as conversion
 * from note names to frequencies and viceversa.
 * Some operations on guitar "structure" are needed too, e.g. the conversion
 * from notes to fret numbers.
 *
 * Semitones are used as common unit for these utilities, because they are the
 * the most convenient way to do operations: semitones are numbers, opposed to
 * common notations for musical notes, which would be strings in C and they are
 * a linear scale, opposed to frequencies, which are an exponential scale.
 *
 * Another reason to choose semitones is that on guitar the difference between a
 * fret and the next one is one semitone.
 *
 * Semitones are a relative unit, but they are used as absolute unit in this
 * project, too, and A0 (first note of the piano) is used as semitone 0, in this
 * case.
 */

#ifndef __GUITAR_H
#define __GUITAR_H

/// SHRT_MIN
#include <limits.h>

/**
 * @brief A type used for operations on semitones.
 *
 * Audible notes range is from C0 (16.35Hz, -9 semitones from A0) to E10
 * (19912.12Hz, 115 semitones from A0).
 * Signed characters would suffice, but shorts have been chosen in order to have
 * a bigger domain to perform calculations without risking to overflow.
 *
 * The notations "semitone_t" is used in order to maintain consisntency with C
 * standard integer typedefs.
 */
typedef short semitone_t;

/**
 * @brief A constant used to tell that the semitone is not valid.
 */
#define INVALID_SEMITONE SHRT_MIN;

/**
 * @brief The number of string in a standard guitar.
 *
 * This value is kept as a paramter so that the program can be adapted to other
 * instruments.
 */
#define GUITAR_STRINGS 6

/**
 * @brief The default number of frets in a standard guitar.
 */
#define GUITAR_FRETS 22

/**
 * @brief Guitar standard tuning in semitones from A0.
 * @link https://en.wikipedia.org/wiki/Standard_tuning
 */
extern const semitone_t STANDARD_TUNING[GUITAR_STRINGS];

/**
 * @brief Convert a note name to semitones relative to A0.
 * @note This function uses English notation for notes (A ... G).
 * @note B#, Cb, E# and Fb will be considered valid notes, whereas multiple #
 *  and b will be considered invalid.
 *
 * @param name The name of the note. A number between 2 or 3 characters will be
 *  read. if this is not a valid note name, INVALID_SEMITONE will be returned.
 * @param octave The octave of the note
 * @return The semitones of the note from A0 or INVALID_SEMITONE in case of
 *  illegal arguments
 */
extern semitone_t noteToSemitones(const char *name, semitone_t octave);

/**
 * @brief Convert a note frequency to semitones relative to A0.
 *
 * This conversion is based on the formula f = f_A0 * 2^(n/12), where f is the
 * frequency of the note, f_A0 is the frequency of A0, n is the number of
 * semitones between A0 and the passed note, and, for this function, the return
 * value.
 *
 * Note that an approximation will be made, however the caller can obtain the
 * approximation error using the second parameter of the function (or pass a
 * null pointer to ignore this feature).
 *
 * @param frequency The note frequency
 * @param error The approximation error. Pass 0 to ignore this feature of the
 *  function
 * @return The semitones of the note from A0
 */
extern semitone_t frequencyToSemitones(float frequency, float *error);

/**
 * @brief Get all the frets in which a note can be played in a guitar.
 *
 * Notes on guitar can be played in different positions, so this function will
 * return all of them for a given note.
 *
 * @note On frets array values, 0 means that the note can be played as open
 * string, whereas a negative value means that the note cannot be played in that
 * string.
 *
 * @param note The note
 * @param tuning The tuning of the guitar
 * @param frets The output array on which the frets will be saved. It must be
 *  preallocated from the caller and have the same number of elements of tuning
 *  array
 * @param strings The number of strings of the guitar, which is the number of
 *  elements of the tuning array, too
 * @param fretsNumber The number of frets of the guitar
 * @return The number of frets in which the note can be played
 */
extern unsigned int noteToFrets(semitone_t note, const semitone_t *tuning,
		semitone_t *frets, unsigned int strings, unsigned int fretsNumber);

#endif /* __GUITAR_H */

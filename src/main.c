/**
 * @file main.c
 * @brief Manages the flow of the program
 *
 * This is the main file of the program. It contains the entry point and from
 * there initializes and runs all the other parts of it.
 */

/// All audio related functions
#include "audio.h"

/// getchar, fprintf, printf
#include <stdio.h>

#include <pthread.h>

static void *waitChar(void *run);

/**
 * @brief The entry point of the program
 *
 * @param argc The number of arguments received
 * @param argv The arguments
 * @return The status code to pass to the OS at the exit.
 */
int main(int argc, char *argv[])
{
	/// The exit status code of the program
	int status = 0;

	/// The flag to keep the recording running
	char run = 1;

	/// The thread that will wait for user input
	pthread_t waitThread;

	AudioContext audio;

	if(audioInit(&audio)) {
		fprintf(stderr, "An error occurred initializating the audio system.\n");
		status = 1;
	}

	if(!status && pthread_create(&waitThread, NULL, waitChar, &run)) {
		fprintf(stderr, "An error occurred while creating the waiting thread.\n");
		status = 2;
	}

	if(!status) {
		status = audioRecord(&audio, &run) ? 3 : 0;
	}

	pthread_join(waitThread, NULL);

	audioClose(&audio);

	return status;
}

/**
 * @brief Wait for the user to input a char through the stdin to stop recording.
 *
 * @param run The pointer to the running flag
 */
static void *waitChar(void *run)
{
	printf("Premere invio per interrompere la registrazione.\n");

	int c;
	while((c = getchar()) != '\n' && c != EOF);

	*((char *) run) = 0;
}

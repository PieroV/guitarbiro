/**
 * @file main.c
 * @brief Manages the flow of the program
 *
 * This is the main file of the program. It contains the entry point and from
 * there initializes and runs all the other parts of it.
 */

#include "audio.h"
#include "gui.h"

/// fprintf
#include <stdio.h>

#include <gtk/gtk.h>

/**
 * @brief The entry point of the program
 *
 * @param argc The number of arguments received
 * @param argv The arguments
 * @return The status code to pass to the OS at the exit.
 */
int main(int argc, char *argv[])
{
	GUIContext *ctx;
	AudioContext *audio;

	audio = audioInit();
	if(!audio) {
		fprintf(stderr, "An error occurred initializating the audio system.\n");
		return 1;
	}

	gtk_init(&argc, &argv);

	if(!(ctx = guiInitMain(audio))) {
		fprintf(stderr, "Errors occurred while trying to initilize the GUI.\n");
		return 2;
	}

	gtk_main();

	guiFree(ctx);
	audioClose(audio);

	return 0;
}

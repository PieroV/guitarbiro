#include "gui.h"

/// fprintf, snprintf
#include <stdio.h>

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

/**
 * @brief The path to the XML files that describes the GUI
 */
static const char *MAIN_WINDOW_FILE = "resources/main_window.glade";

/**
 * @brief The name of the main window in the XML file
 */
static const char *MAIN_WINDOW_NAME = "MainWindow";

/**
 * @brief The path to the guitar neck SVG file
 *
 * This file contains the figure of a guitar and a dot in each fret of each
 * string.
 * The neck is always shown, whereas dots are shown only when that note is
 * played.
 */
static const char *GUITAR_NECK_FILE = "resources/guitar_neck.svg";

/**
 * @brief The name of the layar that contains the neck of the guitar in the SVG
 *
 * To show only the neck, it has been drawn in its own layer, and this is its
 * name.
 */
static const char *GUITAR_NECK_LAYER = "#layer1";

/**
 * @brief The template to get the ID of dots in the guitar neck
 *
 * Each dot has its own id, which contains both the string number (from 1 to 6)
 * and the fret number (from 0 = open string to 22).
 * This string is intended to be used with sprintf.
 * It contains two integer placeholders: the former is used for the string
 * number, the latter for the fret number.
 */
static const char *GUITAR_DOTS_TEMPLATE = "#dot_%d_%d";

/**
 * @brief The size to alloc for dots id
 * @note It must include the space for the null byte
 */
#define GUITAR_DOTS_MAXLENGTH 12

/**
 * @brief The id of the drawing area where the guitar neck is drawn.
 */
static const char *GUITAR_AREA_ID = "NeckArea";

/**
 * @brief Handle the window close signal
 */
static void windowClose();

/**
 * @brief Handle the start recording signal
 *
 * @param button The button that generates the event
 * @param userData Compulsory but not used parameter
 */
static void startRecording(GtkButton *button, gpointer userData);

/**
 * @brief Callback function that just queues the redraw of the neck drawing area
 *
 * GTK isn't really thread safe, however it provides a way to get updates from
 * other threads, using the gdk_threads_add_idle function, which needs a
 * callback that will do operations on GTK objects.
 * In our case, queueRedraw is the callback, and just queues the redraw for the
 * drawing area that contains the guitar neck.
 *
 * Note that the function that wants to queue the redraw has to change gFrets
 * according to the changes it needs.
 * The userData parameter could have been used, but memory leaks prevention
 * would have been more difficult than using the global variable.
 *
 * @param userData A compulsory but not used parameter
 * @return Always return G_SOURCE_REMOVE to run this function once
 */
static gboolean queueRedraw(gpointer userData);

/**
 * @brief The array that contains the frets to highlight
 *
 * We use a global variable because we want to keep the API simple for other
 * parts of the program, and because the GTK API to use parameters on singals is
 * very pedantic.
 *
 * Note that changes on this variable aren't synchronized with mutex or similar,
 * however the GUI thread only reads it, and there should be only another thread
 * that manages the data acquisition and freqeuency detection, that writes this
 * variable, so actually the synchronization shouldn't be needed.
 */
static short gFrets[GUITAR_STRINGS] = {-1, -1, -1, -1, -1, -1};

/**
 * @brief The drawing area where the guitar neck is drawn.
 *
 * Since we want to offer an API that can be called by any part of the program,
 * we don't have any other way to share this pointer between functions.
 */
static GtkWidget *gDrawArea;

static void *randomRedraw(void *data);
#include <unistd.h>
#include <stdlib.h>

/**
 * @brief Handle redraw events
 *
 * @param widget The widget that created the event
 * @param cr The cairo context where the image will be rendered
 * @param userData Compulsory but not used parameter
 */
static void drawGuitar(GtkWidget *widget, cairo_t *cr, gpointer userData);

int guiInitMain()
{
	GtkBuilder *builder;
	GtkWidget *window;

	builder = gtk_builder_new_from_file(MAIN_WINDOW_FILE);

	if(!builder) {
		fprintf(stderr, "Could not create the GTK builder.\n");
		return 0;
	}

	window = GTK_WIDGET(gtk_builder_get_object(builder, MAIN_WINDOW_NAME));
	if(!window) {
		fprintf(stderr, "Could not find the main window data.\n");
		g_object_unref(builder);

		return 0;
	}

	gDrawArea = GTK_WIDGET(gtk_builder_get_object(builder, GUITAR_AREA_ID));
	if(!gDrawArea) {
		fprintf(stderr, "Could not find the guitar neck drawing area.\n");
		g_object_unref(builder);
		g_object_unref(window);

		return 0;
	}

	gtk_builder_add_callback_symbol(builder, "windowClose",
			(GCallback) windowClose);
	gtk_builder_add_callback_symbol(builder, "startRecording",
			(GCallback) startRecording);
	gtk_builder_add_callback_symbol(builder, "drawGuitar",
			(GCallback) drawGuitar);
	gtk_builder_connect_signals(builder, NULL);

	g_object_unref(builder);

	gtk_widget_show(window);

	return 1;
}

void guiHighlightFrets(semitone_t *frets)
{
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		gFrets[i] = frets[i];
	}
}

void guiResetHighlights()
{
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		gFrets[i] = -1;
	}
}

void windowClose()
{
    gtk_main_quit();
}

void startRecording(GtkButton *button, gpointer userData)
{
	static char recordingToggle = 0;

	if(recordingToggle) {
		printf("Start recording.\n");
		gtk_button_set_label(GTK_BUTTON(button), "gtk-media-stop");
	} else {
		printf("Stop recording.\n");
		gtk_button_set_label(GTK_BUTTON(button), "gtk-media-record");
	}

	recordingToggle = !recordingToggle;
}

void drawGuitar(GtkWidget *widget, cairo_t *cr, gpointer userData)
{
	guint width, height;
	GtkStyleContext *context;
	RsvgHandle *rsvgHandle;
	RsvgDimensionData dimensionData;
	float ratio;

	context = gtk_widget_get_style_context(widget);
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	rsvgHandle = rsvg_handle_new_from_file(GUITAR_NECK_FILE, NULL);
	if(!rsvgHandle) {
		fprintf(stderr, "Could not open the neck file.\n");
		return;
	}

	rsvg_handle_get_dimensions(rsvgHandle, &dimensionData);
	ratio = dimensionData.width / (float) dimensionData.height;

	if((ratio * height) < width) {
		ratio = height / (float) dimensionData.height;
	} else {
		ratio = width / (float) dimensionData.width;
	}
	cairo_scale(cr, ratio, ratio);

	rsvg_handle_render_cairo_sub(rsvgHandle, cr, GUITAR_NECK_LAYER);

	char dotId[GUITAR_DOTS_MAXLENGTH];
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		if(gFrets[i] >= 0 && gFrets[i] <= GUITAR_FRETS) {
			// i + 1 because string number goes from 1 to 6 in SVG file
			snprintf(dotId, GUITAR_DOTS_MAXLENGTH, GUITAR_DOTS_TEMPLATE, i + 1,
					gFrets[i]);
			rsvg_handle_render_cairo_sub(rsvgHandle, cr, dotId);
		}
	}

	g_object_unref(rsvgHandle);
}

void *randomRedraw(void *data)
{
	srand(time(NULL));

	while(1) {
		for(int i = 0; i < 6; i++) {
			gFrets[i] = -1;
		}
		gFrets[rand() % 6] = rand() % 23;

		gdk_threads_add_idle(queueRedraw, 0);
		sleep(rand() % 3);
	}
}

gboolean queueRedraw(gpointer userData)
{
	if(gDrawArea) {
		gtk_widget_queue_draw(gDrawArea);
	}

	return G_SOURCE_REMOVE;
}

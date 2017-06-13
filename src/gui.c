#include "gui.h"

/// fprintf, snprintf
#include <stdio.h>
/// malloc, free
#include <stdlib.h>

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

/**
 * @brief The path to the XML file that describes the GUI
 */
static const char *GUI_FILE = "resources/main_window.glade";

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
 * @brief Print an error on standard error if a pointer is null and return 0
 */
#define CHECK_WIDGET(ptr, name) if(!(ptr)) { \
		fprintf(stderr, "Could not find the %s.\n", name); \
		return 0; \
	}

struct _GUIContext {
	/// The main window of the program
	GtkWidget *mainWindow;

	/// A pointer to the settings window
	GtkWidget *settingsWindow;

	/// The apply button on the settings dialog
	GtkWidget *applyButton;

	/// A pointer to the list of backends of the settings dialog
	GtkComboBoxText *backendList;

	/// A pointer to the list of devices of the settings dialog
	GtkComboBoxText *deviceList;
};

/**
 * @brief Populate the GUI context taking from the builder the required widgets
 *
 * @param ctx An instance of the GUI context
 * @param builder The builder of the GUI
 * @return Did the operation succeed?
 */
static int populateContext(GUIContext *ctx, GtkBuilder *builder);

/**
 * @brief Connects the needed signals to GTK.
 *
 * @param ctx An instance of the GUI context
 * @param builder The builder of the GUI
 */
static void connectSignals(GUIContext *ctx, GtkBuilder *builder);

/**
 * @brief Handle the window close signal
 */
static void windowClose();

/**
 * @brief Handle the start recording signal
 *
 * @param button The button that triggered the event
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
 * @brief Handle redraw events
 *
 * @param widget The widget that triggered the event
 * @param cr The cairo context where the image will be rendered
 * @param userData Compulsory but not used parameter
 */
static void drawGuitar(GtkWidget *widget, cairo_t *cr, gpointer userData);

/**
 * @brief Callback to quit using the file menu
 *
 * @param menuItem The item that triggered the event
 * @param settingsWindow The pointer to the main window
 */
static void menuQuit(GtkMenuItem *menuItem, gpointer mainWindow);

/**
 * @brief Callback to open the preferences window.
 *
 * @param menuItem The item that triggered the event
 * @param settingsWindow The pointer to the preferences window
 */
static void menuShowSettings(GtkMenuItem *menuItem, gpointer settingsWindow);

/**
 * @brief Callback that handles the pression of the apply button on settings.
 *
 * @param button The button that triggered the event
 * @param contextPtr A pointer to the GUIContext instance
 */
static void settingsApply(GtkButton *button, gpointer contextPtr);

/**
 * @brief Callback that handles the pression of the cancel button on settings.
 *
 * @param button The button that triggered the event
 * @param settingsWindow A pointer to the settings window
 */
static void settingsCancel(GtkButton *button, gpointer settingsWindow);

/**
 * @brief Callback that handles changes on the list of backends in settings.
 *
 * @param backendList The list that triggered the event
 * @param deviceListPtr The pointer to the list of devices in settings
 */
static void settingsBackendChanged(GtkComboBox *backendList,
		gpointer deviceListPtr);

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

GUIContext *guiInitMain()
{
	GUIContext *ctx;
	GtkBuilder *builder;

	ctx = malloc(sizeof(GUIContext));
	if(!ctx) {
		fprintf(stderr, "Could not allocate the GUIContext.\n");
		return 0;
	}

	builder = gtk_builder_new_from_file(GUI_FILE);
	if(!builder) {
		fprintf(stderr, "Could not create the GTK builder.\n");
		free(ctx);
		return 0;
	}

	if(!populateContext(ctx, builder)) {
		// Errors in this case will be reported by populateContext
		g_object_unref(builder);
		free(ctx);
		return 0;
	}

	connectSignals(ctx, builder);

	// Temporary here, but wil be moved
	gtk_combo_box_text_append(ctx->backendList, NULL, "ALSA");
	gtk_combo_box_text_append(ctx->backendList, NULL, "Pulse");
	gtk_combo_box_text_append(ctx->backendList, NULL, "OSS");
	gtk_combo_box_text_append(ctx->deviceList, NULL, "Pulse Mic 1");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->backendList), 1);

	g_object_unref(builder);

	gtk_widget_show(ctx->mainWindow);

	return ctx;
}

void guiFree(GUIContext *context)
{
	// Just need to free the structure, because GTK will free anything else
	free(context);
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

int populateContext(GUIContext *ctx, GtkBuilder *builder)
{
	ctx->mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "MainWindow"));
	CHECK_WIDGET(ctx->mainWindow, "main window")

	ctx->settingsWindow = GTK_WIDGET(gtk_builder_get_object(builder,
			"SettingsWindow"));
	CHECK_WIDGET(ctx->settingsWindow, "settings window")

	ctx->applyButton = GTK_WIDGET(gtk_builder_get_object(builder,
			"settingsApply"));
	CHECK_WIDGET(ctx->applyButton, "button to save settings")

	ctx->backendList = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder,
			"backendsList"));
	CHECK_WIDGET(ctx->backendList, "list of backends (in settings dialog)")

	ctx->deviceList = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder,
			"devicesList"));
	CHECK_WIDGET(ctx->deviceList, "list of devices (in settings dialog)")

	// The only exception that actually is not stored on the context
	gDrawArea = GTK_WIDGET(gtk_builder_get_object(builder, "NeckArea"));
	CHECK_WIDGET(gDrawArea, "guitar neck drawing area")

	return 1;
}

void connectSignals(GUIContext *ctx, GtkBuilder *builder)
{
	gtk_builder_add_callback_symbol(builder, "windowClose",
			G_CALLBACK(windowClose));
	gtk_builder_add_callback_symbol(builder, "startRecording",
			G_CALLBACK(startRecording));
	gtk_builder_add_callback_symbol(builder, "drawGuitar",
			G_CALLBACK(drawGuitar));
	gtk_builder_add_callback_symbol(builder, "menuQuit",
			G_CALLBACK(menuQuit));
	gtk_builder_add_callback_symbol(builder, "menuShowSettings",
			G_CALLBACK(menuShowSettings));
	gtk_builder_add_callback_symbol(builder, "settingsCancel",
			G_CALLBACK(settingsCancel));
	gtk_builder_add_callback_symbol(builder, "settingsBackendChanged",
			G_CALLBACK(settingsBackendChanged));
	gtk_builder_connect_signals(builder, NULL);

	g_signal_connect(G_OBJECT(ctx->applyButton), "clicked",
			G_CALLBACK(settingsApply), ctx);
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

void menuQuit(GtkMenuItem *menuItem, gpointer mainWindow)
{
	gtk_widget_destroy(GTK_WIDGET(mainWindow));
	gtk_main_quit();
}

void menuShowSettings(GtkMenuItem *menuItem, gpointer settingsWindow)
{
	gtk_window_present(GTK_WINDOW(settingsWindow));
}

void settingsApply(GtkButton *button, gpointer contextPtr)
{
	GUIContext *ctx = (GUIContext *) contextPtr;

	printf("Doing something to apply settings...\n");
	printf("Backend: %s\n", gtk_combo_box_text_get_active_text(ctx->backendList));
	printf("Device: %s\n", gtk_combo_box_text_get_active_text(ctx->deviceList));

	gtk_widget_hide(ctx->settingsWindow);
}

void settingsCancel(GtkButton *button, gpointer settingsWindow)
{
	gtk_widget_hide(GTK_WIDGET(settingsWindow));
}

void settingsBackendChanged(GtkComboBox *backendList, gpointer deviceListPtr)
{
	GtkComboBoxText *deviceList = GTK_COMBO_BOX_TEXT(deviceListPtr);
	GtkComboBox *deviceBox = GTK_COMBO_BOX(deviceListPtr);
	if(!deviceList) {
		return;
	}

	gint id = gtk_combo_box_get_active(backendList);

	if(id == 0) {
		gtk_combo_box_text_remove_all(deviceList);
		gtk_combo_box_text_append(deviceList, NULL, "Alsa Mic 1");
		gtk_combo_box_text_append(deviceList, NULL, "Alsa Mic 2");
		gtk_combo_box_set_active(deviceBox, 1);
	} else if(id == 1) {
		gtk_combo_box_text_remove_all(deviceList);
		gtk_combo_box_text_append(deviceList, NULL, "Pulse Mic 1");
		gtk_combo_box_set_active(deviceBox, 0);
	} else if(id == 2) {
		gtk_combo_box_text_remove_all(deviceList);
		gtk_combo_box_text_append(deviceList, NULL, "/dev/dsp");
		gtk_combo_box_set_active(deviceBox, 0);
	}
}

gboolean queueRedraw(gpointer userData)
{
	if(gDrawArea) {
		gtk_widget_queue_draw(gDrawArea);
	}

	return G_SOURCE_REMOVE;
}

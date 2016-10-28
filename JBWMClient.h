// jbwm - Minimalist Window Manager for X
// Copyright 2008-2016, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#ifndef JBWM_CLIENT_STRUCT_H
#define JBWM_CLIENT_STRUCT_H
#include "JBWMScreen.h"
#include <stdbool.h>
#include <X11/Xutil.h>
struct JBWMClientOptions {
	bool fullscreen : 1;
	bool max_horz : 1;
	bool max_vert : 1;
	bool no_border : 1;
	bool no_close_decor : 1;
	bool no_close : 1;
	bool no_resize_decor : 1;
	bool no_max : 1;
	bool no_min_decor : 1;
	bool no_min : 1;
	bool no_move : 1;
	bool no_resize : 1;
	bool no_title_bar : 1;
	bool remove : 1;
	bool shaded : 1;
	bool shaped : 1;
	bool sticky : 1;
	bool tearoff : 1;
};
#ifdef JBWM_USE_TITLE_BAR
struct JBWMClientTitlebar {
	jbwm_window_t win, close, resize, shade, stick;
};
#endif//JBWM_USE_TITLE_BAR
struct JBWMClient {
	struct JBWMClient *next;
	XSizeHints size;
	jbwm_rectangle_t old_size;
	jbwm_window_t window, parent;
#ifdef JBWM_USE_TITLE_BAR
	struct JBWMClientTitlebar tb;
#endif//JBWM_USE_TITLE_BAR
	jbwm_cmap_t cmap;
#ifdef JBWM_USE_TITLE_BAR
	uint16_t exposed_width;
#endif//JBWM_USE_TITLE_BAR
	int8_t ignore_unmap:3;
	uint8_t vdesk:4;
	uint8_t border:1;
	uint8_t screen;
	struct JBWMClientOptions opt;
};
#endif /* JBWM_CLIENT_STRUCT_H */

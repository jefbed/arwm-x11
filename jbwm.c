// jbwm - Minimalist Window Manager for X
// Copyright 2008-2016, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#include "jbwm.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#ifdef JBWM_USE_XFT
#include <X11/Xft/Xft.h>
#endif//JBWM_USE_XFT
#include "JBWMScreen.h"
#include "config.h"
#include "display.h"
#include "events.h"
#include "ewmh.h"
#include "keys.h"
#include "log.h"
#include "new.h"
#include "util.h"
//#define DEBUG_JBWM
#ifndef DEBUG_JBWM
#undef JBWM_LOG
#define JBWM_LOG(...)
#endif//!DEBUG_JBWM
// Macros:
#define ENV(e) JBWM_ENV_##e
static struct JBWMScreen * screens;
struct JBWMScreen * jbwm_get_screens(void)
{
	return screens;
}
static void print(const size_t sz, const char * buf)
{
	if (write(1, buf, sz) == -1)
		abort();
}
/* Used for overriding the default WM modifiers */
__attribute__((warn_unused_result))
static uint16_t parse_modifiers(char * arg)
{
	JBWM_LOG("parse_modifiers()");
	const size_t s = strlen(arg);
	if (s > 3) { // switch based on the 4th character
		switch(arg[3]) {
		case 'f': // shift
			return ShiftMask;
		case 'k': // lock
			return LockMask;
		case 't': // control
		case 'l': // ctrl
			return ControlMask;
		case '2': // mod2
			return Mod2Mask;
		case '3': // mod3
			return Mod3Mask;
		case '4': // mod4
			return Mod4Mask;
		case '5': // mod5
			return Mod5Mask;
		}
	}
	// everything else becomes mod1
	return Mod1Mask;
}
static void parse_argv(uint8_t argc, char **argv)
{
	JBWM_LOG("parse_argv(%d,%s...)", argc, argv[0]);
	static const char optstring[] = "1:2:b:d:F:f:hs:Vv";
	int8_t opt;
	while((opt=getopt(argc, argv, optstring)) != -1)
		switch (opt) {
		case '1':
			jbwm_set_grab_mask(parse_modifiers(optarg));
			break;
		case '2':
			jbwm_set_mod_mask(parse_modifiers(optarg));
			break;
		case 'b':
			setenv(JBWM_ENV_BG, optarg, 1);
			break;
		case 'd':
			setenv(JBWM_ENV_DISPLAY, optarg, 1);
			break;
		case 'F':
			setenv(JBWM_ENV_FONT, optarg, 1);
			break;
		case 'f':
			setenv(JBWM_ENV_FG, optarg, 1);
			break;
		case 's':
			setenv(JBWM_ENV_FC, optarg, 1);
			break;
		case 'V':
		case 'v':
			goto version;
		default:
			goto usage;
		}
	return;
usage: {
		size_t l = 0;
		while (argv[0][++l]);
		print(l, *argv);
		print(4, " -[");
		print(sizeof(optstring), optstring);
		print(2, "]\n");
       }
version:
	print(sizeof(VERSION), VERSION);
	print(1, "\n");
	exit(0);
}
__attribute__((noreturn))
void jbwm_error(const char * msg)
{
	size_t count = 0;
	while (msg[++count]);
	print(count, msg);
	print(1, "\n");
	exit(1);
}
static void allocate_colors(Display * d, struct JBWMScreen * restrict s)
{
	const uint8_t n = s->screen;
#define PIX(field, env) s->pixels.field = jbwm_get_pixel(d, n, getenv(env));
	PIX(bg, ENV(BG));
	PIX(fc, ENV(FC));
	PIX(fg, ENV(FG));
#ifdef JBWM_USE_TITLE_BAR
	PIX(close, ENV(CLOSE));
	PIX(resize, ENV(RESIZE));
	PIX(shade, ENV(SHADE));
	PIX(stick, ENV(STICK));
#endif//JBWM_USE_TITLE_BAR
#undef PIX
}
static bool check_redirect(Display * d, const Window w)
{
	XWindowAttributes a;
	XGetWindowAttributes(d, w, &a);
	JBWM_LOG("check_redirect(0x%x): override_redirect: %s, "
		"map_state: %s", (int)w,
		a.override_redirect ? "true" : "false",
		a.map_state == IsViewable ? "IsViewable"
		: "not IsViewable");
	return (!a.override_redirect && (a.map_state == IsViewable));
}
// Free returned data with XFree()
static Window * get_windows(Display * dpy, const Window root,
	uint16_t * win_count)
{
	Window * w, d;
	unsigned int n;
	XQueryTree(dpy, root, &d, &d, &w, &n);
	*win_count = n;
	return (Window *)w;
}
static void setup_clients(Display * d, struct JBWMScreen * s)
{
	uint16_t n;
	Window * w = get_windows(d, s->root, &n);
	JBWM_LOG("Started with %d clients", n);
	while (n--)
		if (check_redirect(d, w[n])) {
			JBWM_LOG("Starting client %d, window 0x%x",
				n, (int)w[n]);
			jbwm_new_client(s, w[n]);
		}
	XFree(w);
}
static void setup_screen_elements(Display * d, const uint8_t i)
{
	struct JBWMScreen * s = screens;
	s->screen = i;
	s->vdesk = 0;
	s->root = RootWindow(d, i);
	s->size.width = DisplayWidth(d, i);
	s->size.height = DisplayHeight(d, i);
}
static void setup_gc(Display * d, struct JBWMScreen * s)
{
	allocate_colors(d, s);
	enum {
		VM = GCFunction | GCSubwindowMode | GCLineWidth
			| GCForeground | GCBackground
	};
	XGCValues gv = {.foreground = s->pixels.fg,
		.background = s->pixels.bg,
		.function = GXxor,
		.subwindow_mode = IncludeInferiors,
		.line_width = 1
	};
	s->gc = XCreateGC(d, s->root, VM, &gv);
}
static void setup_event_listeners(Display * d, const Window root)
{
	enum {
		EMASK = SubstructureRedirectMask | SubstructureNotifyMask |
			EnterWindowMask | PropertyChangeMask
			| ColormapChangeMask
	};
	XChangeWindowAttributes(d, root, CWEventMask,
		&(XSetWindowAttributes){.event_mask = EMASK });
}
static void setup_screen(Display * d, const uint8_t i)
{
	struct JBWMScreen * s = &screens[i];
	setup_screen_elements(d, i);
	setup_gc(d, s);
	setup_event_listeners(d, s->root);
	jbwm_grab_screen_keys(s);
	/* scan all the windows on this screen */
	setup_clients(d, s);
	jbwm_ewmh_init_screen(s);
}
static void jbwm_set_defaults(void)
{
#define SETENV(i) setenv(ENV(i), JBWM_DEF_##i, 0)
	SETENV(FG); SETENV(FC); SETENV(BG); SETENV(TERM);
#ifdef JBWM_USE_TITLE_BAR
	SETENV(CLOSE); SETENV(RESIZE); SETENV(SHADE);
	SETENV(STICK); SETENV(FONT);
#endif//JBWM_USE_TITLE_BAR
#undef SETENV
}
int main(int argc, char **argv)
{
	jbwm_set_defaults();
	parse_argv(argc, argv);
	Display * d = jbwm_open_display();
	uint8_t i = ScreenCount(d);
	// allocate using dynamically sized array on stack
	struct JBWMScreen s[i]; // remains in scope till exit.
	screens = s;
	while (i--)
		setup_screen(d, i);
	jbwm_event_loop(d);
	return 0;
}

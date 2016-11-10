// jbwm - Minimalist Window Manager for X
// Copyright 2008-2016, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#include "new.h"
#include "client.h"
#include "config.h"
#include "ewmh.h"
#include "font.h"
#include "jbwm.h"
#include "keys.h"
#include "log.h"
#include "mwm.h"
#include "screen.h"
#include "shape.h"
#include "title_bar.h"
#include "util.h"
#include <stdlib.h>
#include <X11/Xatom.h>
#ifdef JBWM_USE_EWMH
static uint8_t wm_desktop(Display * d, const jbwm_window_t w, uint8_t vdesk)
{
	uint16_t n;
	unsigned long *lprop = jbwm_get_property(d, w,
		jbwm_ewmh_get_atom(JBWM_EWMH_WM_DESKTOP), &n);
	if (lprop) {
		if (n && lprop[0] < JBWM_MAX_DESKTOPS) // is valid
			vdesk = lprop[0]; // Set vdesk to property value
		else // Set to a valid desktop number:
			jbwm_set_property(d, w,
				jbwm_ewmh_get_atom(JBWM_EWMH_WM_DESKTOP),
				XA_CARDINAL, &vdesk, 1);
		XFree(lprop);
	}
	JBWM_LOG("wm_desktop(): vdesk is %d\n", vdesk);
	return vdesk;
}
#else//!JBWM_USE_EWMH
#define wm_desktop(d, w, vdesk) vdesk
#endif//JBWM_USE_EWMH
#ifdef JBWM_USE_EWMH
__attribute__((nonnull))
static void set_frame_extents(Display * restrict d, struct JBWMClient * c)
{
	// Required by wm-spec 1.4:
	const uint8_t b = c->border;
	jbwm_set_property(d, c->window,
		jbwm_ewmh_get_atom(JBWM_EWMH_FRAME_EXTENTS), XA_CARDINAL,
		(&(jbwm_atom_t[]){b, b, b
		 + (c->opt.no_title_bar ? 0
			 : jbwm_get_font_height()), b}), 4);
}
#else//!JBWM_USE_EWMH
#define set_frame_extents(d, c)
#endif//JBWM_USE_EWMH
__attribute__((nonnull))
static void init_properties(Display * restrict d, struct JBWMClient * c)
{
	jbwm_handle_mwm_hints(d, c);
	c->vdesk = jbwm_get_screens()[c->screen].vdesk;
	c->vdesk = wm_desktop(d, c->window, c->vdesk);
}
static bool get_viewable(Display * restrict d,
	struct JBWMClient * restrict c)
{
	XWindowAttributes attr;
	XGetWindowAttributes(d, c->window, &attr);
	c->cmap = attr.colormap;
	c->size.x = attr.x;
	c->size.y = attr.y;
	c->size.width = attr.width;
	c->size.height = attr.height;
	return attr.map_state == IsViewable;
}
static void center(struct JBWMClient * restrict c)
{
	struct JBWMRectangle * restrict g = &c->size;
	if (g->x || g->y)
		return; // geometry has been set
	const struct JBWMSize ss = jbwm_get_screens()[c->screen].size;
	g->x = (ss.w >> 1) - (g->width >> 1);
	g->y = (ss.h >> 1) - (g->height >> 1);
}
__attribute__((nonnull))
static void init_geometry(Display * restrict d, struct JBWMClient * c)
{
	// Test if the reparent that is to come would trigger an unmap event.
	if (get_viewable(d, c))
		++c->ignore_unmap;
	center(c);
}
__attribute__((nonnull))
static Window get_parent(Display * restrict d, struct JBWMClient * restrict c)
{
	return XCreateWindow(d, jbwm_get_screens()[c->screen].root,
		c->size.x, c->size.y, c->size.width, c->size.height,
		c->border, CopyFromParent, CopyFromParent, CopyFromParent,
		CWOverrideRedirect | CWEventMask, &(XSetWindowAttributes){
		.override_redirect=true, .event_mask
		= SubstructureRedirectMask | SubstructureNotifyMask
		| ButtonPressMask | EnterWindowMask});
}
__attribute__((nonnull))
static void reparent(Display * restrict d,
	struct JBWMClient * c) // use of restrict on client here is wrong
{
	JBWM_LOG("reparent()");
	jbwm_new_shaped_client(d, c);
	const jbwm_window_t p = c->parent = get_parent(d, c), w = c->window;
	XAddToSaveSet(d, w);
	XReparentWindow(d, w, p, 0, 0);
	XMapWindow(d, w);
}
// Allocate the client structure with some defaults set
static struct JBWMClient * get_JBWMClient(const jbwm_window_t w,
	struct JBWMScreen * s)
{
	struct JBWMClient * c = malloc(sizeof(struct JBWMClient));
	*c = (struct JBWMClient) {.screen = s->screen,
		.window = w, .border = 1, .next = jbwm_get_head_client()};
	return c;
}
// Grab input and setup JBWM_USE_EWMH for client window
static void do_grabs(Display * restrict d, const jbwm_window_t w)
{
	XSelectInput(d, w, EnterWindowMask
		| PropertyChangeMask | ColormapChangeMask);
	jbwm_grab_window_keys(d, w);
	jbwm_ewmh_set_allowed_actions(d, w);
}
void jbwm_new_client(Display * restrict d, struct JBWMScreen * restrict s,
	const jbwm_window_t w)
{
	JBWM_LOG("jbwm_new_client(%d,s)", (int)w);
	struct JBWMClient * c = get_JBWMClient(w, s);
	jbwm_set_head_client(c);
	do_grabs(d, w);
	init_properties(d, c);
	init_geometry(d, c);
	reparent(d, c);
	set_frame_extents(d, c);
	jbwm_restore_client(d, c);
	jbwm_update_title_bar(d, c);
}

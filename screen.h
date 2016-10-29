// jbwm - Minimalist Window Manager for X
// Copyright 2008-2016, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#ifndef JBWM_SCREEN_H
#define JBWM_SCREEN_H
#include "JBWMClient.h"
void jbwm_drag(Display * restrict d,
	struct JBWMClient * restrict c, const bool resize)
	__attribute__((nonnull));
void jbwm_move_resize(Display * restrict d,
	struct JBWMClient * restrict c)
	__attribute__((nonnull));
uint8_t jbwm_set_vdesk(Display * restrict d,
	struct JBWMScreen * s, uint8_t v)
	__attribute__((nonnull));
void jbwm_restore_client(Display * restrict d,
	struct JBWMClient * restrict c)
	__attribute__((nonnull));
#endif /* JBWM_SCREEN_H */

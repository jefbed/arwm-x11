// jbwm - Minimalist Window Manager for X
// Copyright 2008-2017, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#include "vdesk.h"
#include <X11/Xatom.h> // keep
#include "JBWMAtomIndex.h"
#include "JBWMClient.h"
#include "JBWMScreen.h"
#include "client.h"
#include "config.h"
#include "ewmh.h"
#include "log.h"
#include "util.h"
static void check_visibility(const struct JBWMScreen * s, struct
	JBWMClient * restrict c, const uint8_t v)
{
	if (!c)
		return;
	if (c->screen == s->id) {
		if (c->vdesk == v || c->opt.sticky) {
			// allow moving windows by sticking
			c->vdesk = v;
			jbwm_restore_client(c);
		} else
			jbwm_hide_client(c);
	}
	check_visibility(s, c->next, v);
}
uint8_t jbwm_set_vdesk(Display * d, struct JBWMScreen * s, uint8_t v)
{
	JBWM_LOG("jbwm_set_vdesk");
	if (v == s->vdesk || v > JBWM_MAX_DESKTOPS)
		return s->vdesk;
	check_visibility(s, jbwm_get_head_client(), v);
	s->vdesk = v;
	jbwm_set_property(d, RootWindow(d, s->id),
		jbwm_ewmh_get_atom(JBWM_EWMH_CURRENT_DESKTOP),
		XA_CARDINAL, &v, 1);
	return s->vdesk;
}

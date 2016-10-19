// jbwm - Minimalist Window Manager for X
// Copyright 2008-2016, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <jbwm@6809.org.uk>
// See README for license and other details.
#include "shape.h"
#include "JBWMEnv.h"
#include "log.h"
#include <X11/extensions/shape.h>
// Declared with pure attribute, as value may not change between calls.
__attribute__((pure))
static bool is_shaped(struct JBWMClient * c)
{
	int s, d;
	unsigned int u;
	return XShapeQueryExtents(jbwm.d, c->window, &s, &d, &d,
		&u, &u, &d, &d, &d, &u, &u) && s;
}
void jbwm_set_shape(struct JBWMClient * c)
{
	if(c->opt.shaped) {
		JBWM_LOG("XShapeCombineShape: %d", (int)c->window);
		XShapeCombineShape(jbwm.d, c->parent, ShapeBounding,
			1, 1, c->window, ShapeBounding, ShapeSet);
	}
}
void jbwm_new_shaped_client(struct JBWMClient * c)
{
	if (is_shaped(c)) {
		JBWM_LOG("Window %d is shaped", (int)c->window);
		c->border = 0;
		c->opt.no_titlebar=c->opt.shaped=true;
	}
}

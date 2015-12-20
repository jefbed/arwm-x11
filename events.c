/* jbwm - Minimalist Window Manager for X
 * Copyright (C) 1999-2006 Ciaran Anscomb <jbwm@6809.org.uk>
 * see README for license and other details. */

#include <stdlib.h>
#include "jbwm.h"

static ScreenInfo *
find_screen(Window root)
{
	ubyte i;

	for(i = 0; i < jbwm.X.num_screens; i++)
		if(jbwm.X.screens[i].root == root)
			return &jbwm.X.screens[i];

	return NULL;
}

static void
handle_map_request(XMapRequestEvent * e)
{
	Client *c = find_client(e->window);

	LOG("handle_map_request(e)");
	if(c)
	{
		/* Avoid mapping any ghost windows.  */
		if(c->flags & JB_CLIENT_REMOVE)
			return;
		unhide(c);
	}
	else
	{
		XWindowAttributes attr;

		XGetWindowAttributes(jbwm.X.dpy, e->window, &attr);
		make_new_client(e->window, find_screen(attr.root));
	}
}

static void
cleanup()
{
	Client *c, *i;

	LOG("cleanup()");
	jbwm.need_cleanup=0;
	for(c = head_client; c; c = i)
	{
		i = c->next;
		if(c->flags & JB_CLIENT_REMOVE)
			remove_client(c);
		if(!i)
			return;
	}
}

static void
handle_unmap_event(XUnmapEvent * e)
{
	Client *c;

	LOG("handle_unmap_event(e)");
	LOG("send_event: %s", e->send_event?"true":"false");
	LOG("from_configure: %s", e->from_configure?"true":"false");
		
	if((c = find_client(e->window)))
	{
		LOG("%d ignores remaining", c->ignore_unmap);
		if(c->ignore_unmap<1)
		{
			LOG("!c->ignore_unmap");
			c->flags |= JB_CLIENT_REMOVE;
			jbwm.need_cleanup=1;
		}
		else
		{
			c->ignore_unmap--;
		}
	}
}

#ifdef USE_CMAP
static void
handle_colormap_change(XColormapEvent * e)
{
	Client *c = find_client(e->window);
	
	LOG("handle_colormap_change(e)");
	if(c && e->new)
	{
		c->cmap = e->colormap;
		XInstallColormap(jbwm.X.dpy, c->cmap);
	}
}
#endif /* USE_CMAP */

#ifdef DEBUG
static const char *
get_atom_name(const Atom a)
{
	char *n;
	static char buf[48];

	n=XGetAtomName(jbwm.X.dpy, a);
	strncpy(buf, n, sizeof(buf));
	XFree(n);
	buf[sizeof(buf)-1]=0;

	return buf;
}
#endif /* DEBUG */
static void
handle_property_change(XPropertyEvent * e)
{
	Client *c;
	const Atom a = e->atom;

	if(!e->window)
	{
		LOG("!e->window");
	}
	c = find_client(e->window);
	if(c)
	{
		LOG("handle_property_change(w=%lx, a=%s[%d])", e->window, 
			get_atom_name(a), (int)a);
		switch(a)
		{
#if 0
		case XA_WM_NORMAL_HINTS:
			LOG("XA_WM_NORMAL_HINTS");
			{
				long __attribute__((unused)) flags;

				XGetWMNormalHints(jbwm.X.dpy, c->window, 
					&(c->size), &flags);
				LOG("geometry=%dx%d+%d+%d\nflags=%d\n", 
					c->size.width, c->size.height, 
					c->size.x, c->size.y, 
					(int)flags);
			}
			return;
#endif
		case XA_WM_HINTS: /* Mainly used for icons */
			return;
#ifdef USE_TBAR
		case XA_WM_NAME:
			LOG("XA_WM_NAME");
			update_titlebar(c);
			return;
#endif /* USE_TBAR */
		default:
#if 0
			if(a==GETATOM("WM_STATE"))
			{
				return;
			}
#endif
			if(a==GETATOM("_NET_WM_OPAQUE_REGION"))
			{
				return;
			}
			if(a==GETATOM("_NET_WM_USER_TIME"))
			{
				return;
			}
			moveresize(c);
		}
	}
}

static void
handle_enter_event(XCrossingEvent * e)
{
	Client *c;

	if((c = find_client(e->window)))
	{
		/* Make sure event is on current desktop and only process
			event on the application window.  */
		if(is_sticky(c))
			goto skip;
		if((c->vdesk != c->screen->vdesk) || (e->window != c->window))
			return;
skip:
		select_client(c);
	}
}

#ifdef USE_TBAR
static void
handle_expose_event(XEvent * ev)
{
	if(ev->xexpose.count == 0)
	{
		/* xproperty was used instead of xexpose, previously.  */
		const Window w = ev->xexpose.window;
		Client *c;

		if(!(c= find_client(w)))
			return;
		if(w==c->titlebar)
			update_titlebar(c);
	}
}
#endif

static void
jbwm_handle_configure_request(XConfigureRequestEvent * e)
{
	Client *c;
	XWindowChanges wc;

	c=find_client(e->window);
	wc.x=e->x;
	wc.y=e->y;
	wc.width=e->width;
	wc.height=e->height;
	wc.border_width=0;
	wc.sibling=e->above;
	wc.stack_mode=e->detail;
	if(c)
	{
		if(e->value_mask & CWStackMode && e->value_mask & CWSibling)
		{
			Client *s;

			s = find_client(e->above);
			if(s)
				wc.sibling=s->parent;
		}
	}
	else
		XConfigureWindow(jbwm.X.dpy, e->window, e->value_mask, &wc);
}

static void
handle_client_message(XClientMessageEvent *e)
{
	ScreenInfo *s;
	Client *c;
	Window cur_root, dw;
	int di;
	unsigned int dui;

	XQueryPointer(jbwm.X.dpy, jbwm.X.screens[0].root, &cur_root, &dw, &di,
		&di, &di, &di, &dui);
	s=find_screen(cur_root);
	if(e->message_type==GETATOM("_NET_WM_DESKTOP"))
	{
		switch_vdesk(s, e->data.l[0]);
		return;
	}
	c=find_client(e->window);
	if(!c)
		return;
	if(e->message_type==GETATOM("_NET_ACTIVE_WINDOW"))
	{
		if(c->screen==s)
			select_client(c);
		return;
	}
	if(e->message_type==GETATOM("_NET_CLOSE_WINDOW"))
	{
		/* Only do this if it came from direct user action */
                if (e->data.l[1] == 2) {
                        send_wm_delete(c);
                }
                return;
	}
	if(e->message_type == GETATOM("_NET_WM_STATE"))
	{
		int i;
		bool m;

		m=false;
		for(i=1; i<=2; i++)
		{	
			if((Atom)e->data.l[i] 
				== GETATOM("_NET_WM_STATE_FULLSCREEN"))
			{
				m=true;
			}
		}
		if(m)
		{
			maximize(c);
		}
	} 
}

void
main_event_loop(void)
{
	XEvent ev;
head:
	XNextEvent(jbwm.X.dpy, &ev);
	switch (ev.type)
	{
	case EnterNotify:
		handle_enter_event(&ev.xcrossing);
		break;
	case UnmapNotify:
		LOG("UnmapNotify");
		handle_unmap_event(&ev.xunmap);
		break;
	case PropertyNotify:
		LOG("PropertyNotify");
		handle_property_change(&ev.xproperty);
		break;
	case MapRequest:
		LOG("MapRequest");
		handle_map_request(&ev.xmaprequest);
		break;
	case KeyPress:
		LOG("KeyPress");
		jbwm_handle_key_event(&ev.xkey);
		break;
#ifdef USE_TBAR
	case Expose:
		LOG("Expose");
		handle_expose_event(&ev);
		break;
#endif
	case ButtonPress:
		LOG("ButtonPress");
		jbwm_handle_button_event(&ev.xbutton);
		break;
	case ConfigureRequest:
		LOG("ConfigureRequest");
		jbwm_handle_configure_request(&ev.xconfigurerequest);
		break;
#ifdef USE_CMAP
	case ColormapNotify:
		LOG("ColormapNotify");
		handle_colormap_change(&ev.xcolormap);
		break;
#endif /* USE_CMAP */
	case ClientMessage:
		handle_client_message(&ev.xclient);
		break;
#ifdef USE_SHAPE
	case ShapeNotify:
		LOG("ShapeNotify");
		set_shape(find_client(ev.xany.window));
		break;
#endif /* USE_SHAPE */
	}
	if(jbwm.need_cleanup)
		cleanup();
	goto head;
}


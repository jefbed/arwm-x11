/*
 * jbwm - Restructuring, optimization, and feature fork
 *        Copyright 2007-2016, Jeffrey E. Bedard <jefbed@gmail.com>
 * evilwm - Minimalist Window Manager for X Copyright (C) 1999-2006 Ciaran
 * Anscomb <jbwm@6809.org.uk> see README for license and other details.
 */

#include "jbwm.h"
#include <unistd.h>

#ifdef EWMH

EWMHEnvironment ewmh;

void
ewmh_init()
{
	// Non-ewmh atoms:
	ewmh.WM_CHANGE_STATE=XA("WM_CHANGE_STATE");
	// Ewmh atoms:
	/* Initializing these elements with the preprocessor shortcut
		below, and following a variable naming convention directly
		corresponding to the atom name, allows further programatic
		access to the atoms defined and helps prevent typographic
		errors.  */
#define EA(atom) ewmh.supported[ewmh.count++]=ewmh.atom=XA("_NET_"#atom)
	EA(SUPPORTED); // Must be first!!!
	EA(CURRENT_DESKTOP);
	EA(NUMBER_OF_DESKTOPS);
	EA(DESKTOP_VIEWPORT);
	EA(DESKTOP_GEOMETRY);
	EA(SUPPORTING_WM_CHECK);
	EA(ACTIVE_WINDOW);
	EA(MOVERESIZE_WINDOW);
	EA(CLOSE_WINDOW);
	EA(CLIENT_LIST);
	EA(CLIENT_LIST_STACKING);
	EA(RESTACK_WINDOW);
	EA(REQUEST_FRAME_EXTENTS);
	EA(VIRTUAL_ROOTS);

	EA(WM_ALLOWED_ACTIONS);
	EA(WM_NAME);
	EA(WM_DESKTOP);
	EA(WM_MOVERESIZE);
	EA(WM_PID);
	EA(WM_MOVERESIZE_CANCEL);
	EA(WM_WINDOW_TYPE);
	EA(WM_STRUT_PARTIAL);

	EA(WM_ACTION_MOVE);
	EA(WM_ACTION_RESIZE);
	EA(WM_ACTION_CLOSE);
	EA(WM_ACTION_SHADE);
	EA(WM_ACTION_FULLSCREEN);
	EA(WM_ACTION_CHANGE_DESKTOP);
	EA(WM_ACTION_ABOVE);
	EA(WM_ACTION_BELOW);
	EA(WM_ACTION_MAXIMIZE_HORZ);
	EA(WM_ACTION_MAXIMIZE_VERT);

	EA(WM_STATE);
	EA(WM_STATE_STICKY);
	EA(WM_STATE_MAXIMIZED_VERT);
	EA(WM_STATE_MAXIMIZED_HORZ);
	EA(WM_STATE_SHADED);
	EA(WM_STATE_HIDDEN);
	EA(WM_STATE_FULLSCREEN);
	EA(WM_STATE_ABOVE);
	EA(WM_STATE_BELOW);
	EA(WM_STATE_DEMANDS_ATTENTION);
	EA(WM_STATE_FOCUSED);
	EA(WM_STATE_SKIP_PAGER);

	LOG("Added %d _NET atoms, %lu allowed", ewmh.count, 
		sizeof(ewmh.supported)/sizeof(Atom));
	// Quit if overflow, sanity check.  
	if(ewmh.count>sizeof(ewmh.supported)/sizeof(Atom))
		ERROR("BufferOverflow");
}

static void
set_desktop_viewport()
{
	const long vp[2]={0,0};
	XPROP(jbwm.X.screens->root, ewmh.DESKTOP_VIEWPORT, XA_CARDINAL, &vp, 2);
}

void
ewmh_update_client_list()
{
#define MAX_CLIENTS 1024
	// Prevent data from disappearing after return.
	static Window wl[MAX_CLIENTS];
	unsigned short count=0;
	for(Client *i=jbwm.head; i&&(count<MAX_CLIENTS); i=i->next)
		wl[count++]=i->window;
	XPROP(jbwm.X.screens->root, ewmh.CLIENT_LIST, XA_WINDOW, &wl, count);
	// FIXME: Does not correctly report stacking order.
	XPROP(jbwm.X.screens->root, ewmh.CLIENT_LIST_STACKING, XA_WINDOW, 
		&wl, count);
}


// Remove specified atom from WM_STATE
void
ewmh_remove_state(const Window w, const Atom state)
{
	unsigned long n;
	Atom *a=get_property(w, ewmh.WM_STATE, XA_ATOM, &n);
	if(!a) return;
	const unsigned long nitems=n;
	while(n--)
		if(a[n]==state)
			a[n]=0;
	XPROP(w, ewmh.WM_STATE, XA_ATOM, &a, nitems);
	XFree(a);
}

bool
ewmh_get_state(const Window w, const Atom state)
{
	unsigned long n;
	Atom *a=get_property(w, ewmh.WM_STATE, XA_ATOM, &n);
	if(!a) return false; 
	bool found=false;
	while(n--)
		if((found=(a[n]==state)))
			break;
	XFree(a);
	return found;
}

void
ewmh_add_state(const Window w, const Atom state)
{
	 XChangeProperty(D, w, ewmh.WM_STATE, XA_ATOM, 32, 
                        PropModePrepend, (unsigned char *)&state, 1);
}

static void
set_number_of_desktops(const Window r)
{
	const long num=DESKTOPS;
	XPROP(r, ewmh.NUMBER_OF_DESKTOPS, XA_CARDINAL, &num, 1);
	set_desktop_viewport();
	XPROP(r, ewmh.VIRTUAL_ROOTS, XA_WINDOW, &r, 1);
}


/* 	Reference, per wm-spec:
  window  = the respective client window
  message_type = _NET_WM_STATE
  format = 32
  data.l[0] = the action, as listed below
  data.l[1] = first property to alter
  data.l[2] = second property to alter
  data.l[3] = source indication:
        _NET_WM_STATE_REMOVE        0    remove/unset property 
        _NET_WM_STATE_ADD           1    add/set property 
        _NET_WM_STATE_TOGGLE        2    toggle property 
  other data.l[] elements = 0 */ 

static void
check_state(XClientMessageEvent *e, // event data
	const Atom state, // state to test
	void *data, // user data
	// callback:
	void (*state_cb)(XClientMessageEvent *, // event data
		bool, // add action if true, else remove action
		void *)) // user data
{
	// 2 atoms can be set at once
	const bool m1=e->data.l[1]==(long)state;
	const bool m2=e->data.l[2]==(long)state;
	if(m1||m2)
	{
		const Window w=e->window;
		switch(e->data.l[0])
		{
		default:
		case 0: // remove
			state_cb(e, false, data);
			ewmh_remove_state(w, state);
			break;
		case 1: // add 
			state_cb(e, true, data);
			ewmh_add_state(w, state);
			break;
		case 2: // toggle
			{
				const bool add=!ewmh_get_state(w, state);
				state_cb(e, add, data);
				if(add)
					ewmh_add_state(w, state);
			}
		}
	}
}

static void
above_cb(XClientMessageEvent *e, bool add, void *data __attribute__((unused)))
{
	if(add) XRaiseWindow(D, e->window);
}

static void
below_cb(XClientMessageEvent *e, bool add, void *data __attribute__((unused)))
{
	if(add) XLowerWindow(D, e->window);
}

static void
skip_pager_cb(XClientMessageEvent *e __attribute__((unused)), bool add, 
	void *data)
{
	if(!data) return;
	Client *c=(Client *)data;
	if(add) add_sticky(c);
	else remove_sticky(c);
}

static void
fullscreen_cb(XClientMessageEvent *e __attribute__((unused)), bool add, 
	void *data)
{
	if(!data) return;
	Client *c=data;
	const bool is_max=c->flags&JB_CLIENT_MAXIMIZED;
	if((add && !is_max) || (!add && is_max))
		maximize(c);
}
    
static void
handle_wm_state_changes(XClientMessageEvent *e, Client *c)
{
	check_state(e, ewmh.WM_STATE_ABOVE, NULL, above_cb);
	check_state(e, ewmh.WM_STATE_BELOW, NULL, below_cb);
	check_state(e, ewmh.WM_STATE_SKIP_PAGER, c, skip_pager_cb);
	check_state(e, ewmh.WM_STATE_FULLSCREEN, c, fullscreen_cb);
	check_state(e, ewmh.WM_STATE_MAXIMIZED_HORZ, c, fullscreen_cb);
	check_state(e, ewmh.WM_STATE_MAXIMIZED_VERT, c, fullscreen_cb);
}

static void
gravitate_border(Client *c, int bw) {
        int dx = 0, dy = 0;
        switch (c->size.win_gravity) {
        default:
        case NorthWestGravity:
                dx = bw;
                dy = bw;
                break;
        case NorthGravity:
                dy = bw;
                break;
        case NorthEastGravity:
                dx = -bw;
                dy = bw;
                break;
        case EastGravity:
                dx = -bw;
                break;
        case CenterGravity:
                break;
        case WestGravity:
                dx = bw;
                break;
        case SouthWestGravity:
                dx = bw;
                dy = -bw;
                break;
        case SouthGravity:
                dy = -bw;
                break;
        case SouthEastGravity:
                dx = -bw;
                dy = -bw;
                break;
        }
        if (c->size.x != 0 || c->size.width 
		!= DisplayWidth(D, c->screen->screen)) 
                c->size.x += dx;
        if (c->size.y != 0 || c->size.height 
		!= DisplayHeight(D, c->screen->screen))
                c->size.y += dy;
}

static void
do_window_changes(int value_mask, XWindowChanges *wc, Client *c, int gravity) 
{
        if (gravity == 0)
                gravity = c->win_gravity_hint;
        c->size.win_gravity = gravity;
        if (value_mask & CWX) c->size.x = wc->x;
        if (value_mask & CWY) c->size.y = wc->y;
        if (value_mask & (CWWidth|CWHeight)) 
	{
                int dw = 0, dh = 0;
                if (!(value_mask & (CWX|CWY))) 
                        gravitate_border(c, -c->border);
                if (value_mask & CWWidth) 
		{
                        int neww = wc->width;
                        if (neww < c->size.min_width)
                                neww = c->size.min_width;
                        if (c->size.max_width && neww > c->size.max_width)
                                neww = c->size.max_width;
                        dw = neww - c->size.width;
                        c->size.width = neww;
                }
                if (value_mask & CWHeight) 
		{
                        int newh = wc->height;
                        if (newh < c->size.min_height)
                                newh = c->size.min_height;
                        if (c->size.max_height && newh > c->size.max_height)
                                newh = c->size.max_height;
                        dh = newh - c->size.height;
                        c->size.height = newh;
                }
                /* only apply position fixes if not being explicitly moved */
                if (!(value_mask & (CWX|CWY))) 
		{
                        switch (gravity) 
			{
                        default:
                        case NorthWestGravity:
                                break;
                        case NorthGravity:
                                c->size.x -= (dw / 2);
                                break;
                        case NorthEastGravity:
                                c->size.x -= dw;
                                break;
                        case WestGravity:
                                c->size.y -= (dh / 2);
                                break;
                        case CenterGravity:
                                c->size.x -= (dw / 2);
                                c->size.y -= (dh / 2);
                                break;
                        case EastGravity:
                                c->size.x -= dw;
                                c->size.y -= (dh / 2);
                                break;
                        case SouthWestGravity:
                                c->size.y -= dh;
                                break;
                        case SouthGravity:
                                c->size.x -= (dw / 2);
                                c->size.y -= dh;
                                break;
                        case SouthEastGravity:
                                c->size.x -= dw;
                                c->size.y -= dh;
                                break;
                        }
                        value_mask |= CWX|CWY;
                        gravitate_border(c, c->border);
                }
        }
        wc->x = c->size.x - c->border;
        wc->y = c->size.y - c->border;
        wc->border_width = c->border;
        XConfigureWindow(D, c->parent, value_mask, wc);
        XMoveResizeWindow(D, c->window, 0, 0, c->size.width, c->size.height);
        if ((value_mask & (CWX|CWY)) && !(value_mask & (CWWidth|CWHeight))) 
		configure(c);
}

void
ewmh_client_message(XClientMessageEvent *e)
{
	const Atom t=e->message_type;
#ifdef DEBUG
        puts("----CLIENTMESSAGE----");
        print_atom(t, __LINE__);
        print_atom(e->data.l[0], __LINE__);
        print_atom(e->data.l[1], __LINE__);
        print_atom(e->data.l[2], __LINE__);
        print_atom(e->data.l[3], __LINE__);
#endif//DEBUG
        ScreenInfo *s = jbwm.X.screens;
	const long val=e->data.l[0];
        if(t==ewmh.CURRENT_DESKTOP) switch_vdesk(s, val);
        Client *c=find_client(e->window);
	if(t==ewmh.WM_DESKTOP && c)
		client_to_vdesk(c, val);
	else if(t==ewmh.DESKTOP_VIEWPORT)
		set_desktop_viewport();
	else if(t==ewmh.NUMBER_OF_DESKTOPS)
		set_number_of_desktops(s->root);
        else if(t==ewmh.ACTIVE_WINDOW && val==2 && c)
		select_client(c);
	else if(t==ewmh.CLOSE_WINDOW && e->data.l[1]==2 && c)
		send_wm_delete(c);
	// If something else moves the window:
	else if(t==ewmh.MOVERESIZE_WINDOW)
	{
		int src=(val>>12) & 3;
		if(src==2)
		{
			XWindowChanges wc={.x=e->data.l[1], .y=e->data.l[2],
				.width=e->data.l[3], .height=e->data.l[4]};
			int vm=(val>>8)&0x0f;
			int grav=val&0xff;
			do_window_changes(vm, &wc, c, grav);
		}
	}
	// If user moves window (client-side titlebars):
        else if(t==ewmh.WM_MOVERESIZE && c)
	{
		XRaiseWindow(D, c->parent);
		drag(c);
	}
        else if(t==ewmh.WM_STATE)
		handle_wm_state_changes(e, c);
	else if(t==ewmh.WM_CHANGE_STATE && c)
	{
		if(val==3) // Minimize (lower)
			XLowerWindow(D, c->parent);
	}
}

void
set_ewmh_allowed_actions(const Window w)
{
	Atom a[]={ewmh.WM_ALLOWED_ACTIONS, ewmh.WM_ACTION_MOVE,
		ewmh.WM_ACTION_RESIZE, ewmh.WM_ACTION_CLOSE,
		ewmh.WM_ACTION_SHADE, ewmh.WM_ACTION_FULLSCREEN,
		ewmh.WM_ACTION_CHANGE_DESKTOP, ewmh.WM_ACTION_ABOVE,
		ewmh.WM_ACTION_BELOW, ewmh.WM_ACTION_MAXIMIZE_HORZ,
		ewmh.WM_ACTION_MAXIMIZE_VERT};
	XPROP(w, a[0], XA_ATOM, &a, sizeof(a)/sizeof(Atom));
}      

void
setup_ewmh_for_screen(ScreenInfo *s)
{      
	const Window r=s->root;
	XPROP(r, ewmh.supported[0], XA_ATOM, ewmh.supported, ewmh.count);
        const unsigned long workarea[4] = { 0, 0, DisplayWidth(D, s->screen),
                DisplayHeight(D, s->screen)};
        XPROP(r, ewmh.DESKTOP_VIEWPORT, XA_CARDINAL, &workarea[0], 2);
        XPROP(r, ewmh.DESKTOP_GEOMETRY, XA_CARDINAL, &workarea[2], 2);
        const unsigned long vdesk = s->vdesk;
 	XPROP(r, ewmh.CURRENT_DESKTOP, XA_CARDINAL, &vdesk, 1);
        const unsigned char n=1;
        XPROP(r, ewmh.NUMBER_OF_DESKTOPS, XA_CARDINAL, &n, 1);
        XPROP(r, ewmh.WM_NAME, XA_STRING, "jbwm", 4);
        s->supporting=XCreateSimpleWindow(D, s->root, 0, 0, 1, 1, 0, 0, 0);
	XPROP(r, ewmh.SUPPORTING_WM_CHECK, XA_WINDOW, &(s->supporting), 1);
	XPROP(s->supporting, ewmh.SUPPORTING_WM_CHECK, XA_WINDOW,
		&(s->supporting), 1);
        XPROP(s->supporting, ewmh.WM_NAME, XA_STRING, "jbwm", 4);
        const pid_t pid=getpid();
        XPROP(s->supporting, ewmh.WM_PID, XA_CARDINAL, &pid, 1);
	// Set this to the root window until we have some clients.  
        XPROP(r, ewmh.CLIENT_LIST, XA_WINDOW, &r, 1);
}      

#endif//EWMH

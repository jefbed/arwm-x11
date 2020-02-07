// jbwm - Minimalist Window Manager for X
// Copyright 2008-2020, Jeffrey E. Bedard <jefbed@gmail.com>
// Copyright 1999-2015, Ciaran Anscomb <evilwm@6809.org.uk>
// See README for license and other details.
//#undef DEBUG
#include "ewmh.h"
#include <assert.h>
#include "JBWMClient.h"
#include "JBWMScreen.h"
#include "client.h"
#include "config.h"
#include "font.h"
#include "geometry.h"
#include "log.h"
#include "macros.h"
#include "PropertyData.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include <X11/Xatom.h>
#define EWMH(a_name) JBWM_EWMH_##a_name
static Atom jbwm_ewmh[JBWM_EWMH_ATOMS_COUNT];
static void jbwm_ewmh_init(Display * d)
{
    static char * jbwm_atom_names [] = {
        "_NET_SUPPORTED",
        "_NET_CURRENT_DESKTOP",
        "_NET_NUMBER_OF_DESKTOPS",
        "_NET_DESKTOP_VIEWPORT",
        "_NET_DESKTOP_GEOMETRY",
        "_NET_SUPPORTING_WM_CHECK",
        "_NET_ACTIVE_WINDOW",
        "_NET_MOVERESIZE_WINDOW",
        "_NET_CLOSE_WINDOW",
        "_NET_CLIENT_LIST",
        "_NET_VIRTUAL_ROOTS",
        "_NET_CLIENT_LIST_STACKING",
        "_NET_FRAME_EXTENTS",
        "_NET_WM_ALLOWED_ACTIONS",
        "_NET_WM_NAME",
        "_NET_WM_DESKTOP",
        "_NET_WM_MOVERESIZE",
        "_NET_WM_PID",
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_STATE",
        "_NET_WM_ACTION_MOVE",
        "_NET_WM_ACTION_RESIZE",
        "_NET_WM_ACTION_CLOSE",
        "_NET_WM_ACTION_SHADE",
        "_NET_WM_ACTION_FULLSCREEN",
        "_NET_WM_ACTION_CHANGE_DESKTOP",
        "_NET_WM_ACTION_ABOVE",
        "_NET_WM_ACTION_BELOW",
        "_NET_WM_ACTION_MAXIMIZE_HORZ",
        "_NET_WM_ACTION_MAXIMIZE_VERT",
        "_NET_WM_STATE_STICKY",
        "_NET_WM_STATE_MAXIMIZED_VERT",
        "_NET_WM_STATE_MAXIMIZED_HORZ",
        "_NET_WM_STATE_SHADED",
        "_NET_WM_STATE_HIDDEN",
        "_NET_WM_STATE_FULLSCREEN",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_STATE_BELOW",
        "_NET_WM_STATE_FOCUSED",
    };
    XInternAtoms(d, jbwm_atom_names, JBWM_EWMH_ATOMS_COUNT,
        false, jbwm_ewmh);
}
static void check_ewmh_init(Display * d)
{
    static bool jbwm_ewmh_init_done;
    if (!jbwm_ewmh_init_done) {
        jbwm_ewmh_init(d);
        jbwm_ewmh_init_done = true;
    }
}
Atom jbwm_ewmh_get_atom(const uint8_t index)
{
    return jbwm_ewmh[index];
}
static inline void set_property(struct PropertyData * restrict p)
{
    jbwm_set_property(p->display, p->target, jbwm_ewmh[p->property],
        p->type, p->data, p->size);
}
static inline void set_ewmh_property(Display * d, const Window win,
    const enum JBWMAtomIndex i, const Atom type,
    void * data, const uint16_t size)
{
    set_property(&(struct PropertyData){.display = d, .data = data,
        .target = win, .property = i, .size = size, .type = type});
}
static inline void set_root_property(Display * d,
    const enum JBWMAtomIndex i, const Atom type,
    void * data, const uint16_t size)
{
    set_ewmh_property(d, DefaultRootWindow(d), i, type, data, size);
}
// returns number of elements in window list
static int get_client_list_r(Window ** list, Display * d,
    struct JBWMClient * i, const int count)
{
    int rval;
    if (i) {
        const int new_count  = count + 1;
        *list = realloc(*list, new_count  * sizeof(Window));
        (*list)[count] =  i->window;
        rval = get_client_list_r(list, d, i->next, new_count);
    } else
        rval = count;
    return rval;
}
#ifdef DEBUG
static void debug_window_list(int n, Window * window_list)
{
    if (n > 0) {
        JBWM_LOG("window_list[%d] is 0x%x",
            n, (int)window_list[n]);
        debug_window_list(n - 1, window_list);
    }
}
#else//!DEBUG
#define debug_window_list(n, w)
#endif//DEBUG
static Window * get_mixed_client_list(Display * d)
{
    static Window * window_list;
    int n;
    if (window_list) {
        free(window_list);
        window_list = NULL;
    }
    n = get_client_list_r(&window_list, d,
        jbwm_get_head_client(), 0);
    set_root_property(d, JBWM_EWMH_CLIENT_LIST, XA_WINDOW,
        window_list, n);
    debug_window_list(n, window_list);
    return window_list;
}
static inline bool query_tree(Display * d, Window ** children_return,
    unsigned int * restrict nchildren_return)
{
    Window parent,root;
    parent=0;
    root=0;
    assert(d);
    assert(children_return);
    assert(nchildren_return);
    return XQueryTree(d, DefaultRootWindow(d), &parent, &root, children_return,
        nchildren_return);
}
static unsigned int get_window_list(Display * d, const uint8_t max_clients,
    Window * window_list)
{
    Window * wl;
    unsigned int n;
    if (query_tree(d, &wl, &n)) {
        n = JB_MIN(n, max_clients); // limit to MAX_CLIENTS
        memcpy(window_list, wl, n * sizeof(Window));
        debug_window_list(n, wl);
        XFree(wl);
    }
    return n;
}
static Window * get_ordered_client_list(Display * d)
{
    enum {MAX_CLIENTS = 64};
    static Window window_list[MAX_CLIENTS];
    // get ordered list of all windows on default screen:
    const unsigned int n = get_window_list(d, MAX_CLIENTS, window_list);
    JBWM_LOG("get_ordered_client_list() n: %d", (int)n);
    set_root_property(d, JBWM_EWMH_CLIENT_LIST_STACKING,
        XA_WINDOW, window_list, n);
    return window_list;
}
void jbwm_ewmh_update_client_list(Display * d)
{
    get_mixed_client_list(d);
    get_ordered_client_list(d);
}
void jbwm_ewmh_set_allowed_actions(Display * d,
    const Window w)
{
    check_ewmh_init(d);
    Atom a[] = {
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_MOVE],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_RESIZE],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_CLOSE],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_SHADE],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_FULLSCREEN],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_CHANGE_DESKTOP],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_ABOVE],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_BELOW],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_MAXIMIZE_HORZ],
        jbwm_ewmh[JBWM_EWMH_WM_ACTION_MAXIMIZE_VERT],
    };
    jbwm_set_property(d, w, a[0], XA_ATOM, &a, sizeof(a) / sizeof(Atom));
}
static void set_desktop_geometry(struct PropertyData * restrict p,
    struct JBWMScreen *s)
{
    p->property = EWMH(DESKTOP_GEOMETRY);
    p->data = (int32_t[]){s->xlib->width,s->xlib->height};
    set_property(p);
}
static void set_desktop_viewport(struct PropertyData * restrict p)
{
    int32_t viewport_data[] = {0, 0};
    p->property = EWMH(DESKTOP_VIEWPORT);
    p->data = viewport_data;
    set_property(p);
}
static void set_number_of_desktops(struct PropertyData * restrict p)
{
    p->property = EWMH(NUMBER_OF_DESKTOPS);
    p->data=&(int32_t){JBWM_MAX_DESKTOPS};
    set_property(p);
}
static void set_current_desktop(struct PropertyData * restrict p,
    void * restrict data)
{
    p->property = EWMH(CURRENT_DESKTOP);
    p->data = data;
    set_property(p);
}
static void set_virtual_roots(struct PropertyData * restrict p)
{
    // Declared r static to keep scope
    static Window r;
    p->property = EWMH(VIRTUAL_ROOTS);
    p->type = XA_WINDOW;
    r = p->target;
    p->data = &r;
    set_property(p);
}
static void init_desktops(Display * d, struct JBWMScreen * s)
{
    struct PropertyData p = {d, NULL, RootWindowOfScreen(s->xlib),
        0, 2, XA_CARDINAL};
    set_desktop_geometry(&p, s);
    set_desktop_viewport(&p);
    p.size = 1; // single dimension data follows
    set_number_of_desktops(&p);
    set_current_desktop(&p, &s->vdesk);
    set_virtual_roots(&p);
}
static void set_name(Display * d, const Window w)
{
    set_ewmh_property(d, w, JBWM_EWMH_WM_NAME, XA_STRING, JBWM_NAME,
        sizeof(JBWM_NAME));
}
static void set_supporting(Display * d, const Window w,
    Window * s)
{
    set_ewmh_property(d, w, JBWM_EWMH_SUPPORTING_WM_CHECK,
        XA_WINDOW, s, 1);
}
static Window init_supporting(Display * d, const Window r)
{
    Window w;
    w = XCreateSimpleWindow(d, r, 0, 0, 1, 1, 0, 0, 0);
    set_supporting(d, r, &w);
    set_supporting(d, w, &w);
    set_ewmh_property(d, w, JBWM_EWMH_WM_PID, XA_CARDINAL,
        &(pid_t){getpid()}, 1);
    set_name(d, w);
    return w;
}
static void set_ewmh_client_list(Display *d, Window *r){
    /* Set this to the root window until we have some clients.
     * Declared r static so we don't lose scope.  */
    set_ewmh_property(d, *r, JBWM_EWMH_CLIENT_LIST, XA_WINDOW, r, 1);
}
static void set_ewmh_supported(Display *d, Window const r){
    set_ewmh_property(d, r, JBWM_EWMH_SUPPORTED, XA_ATOM, jbwm_ewmh,
        JBWM_EWMH_ATOMS_COUNT);
}
void jbwm_ewmh_init_screen(Display * d, struct JBWMScreen * s)
{
    static Window r;
    check_ewmh_init(d);
    r = RootWindowOfScreen(s->xlib);
    set_ewmh_supported(d,r);
    set_name(d, r);
    set_ewmh_client_list(d,&r);
    init_desktops(d, s);
    s->supporting = init_supporting(d, r);
}
// Required by wm-spec:
void jbwm_set_frame_extents(struct JBWMClient * restrict c)
{
    static uint32_t f[4];
    check_ewmh_init(c->display);
    JBWM_LOG("jbwm_set_frame_extents()");
    // Fields: left, right, top, bottom
    f[0] = f[1] = f[2] = f[3] = c->opt.border;
    if (!c->opt.no_title_bar)
        f[2] += jbwm_get_font_height();
    set_ewmh_property(c->display, c->parent, JBWM_EWMH_FRAME_EXTENTS,
        XA_CARDINAL, f, 4);
}

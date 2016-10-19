#ifndef JBWM_EWMH_STATE_H
#define JBWM_EWMH_STATE_H
#ifdef JBWM_USE_EWMH
#include "JBWMClient.h"
void jbwm_ewmh_remove_state(const Window w, const Atom state);
void jbwm_ewmh_add_state(const Window w, Atom state);
void jbwm_ewmh_handle_client_message(XClientMessageEvent * restrict e,
	struct JBWMClient * restrict c)
	__attribute__((nonnull(1)));
#else//!JBWM_USE_EWMH
#define jbwm_ewmh_remove_state(w, s)
#define jbwm_ewmh_add_state(w, s)
#define jbwm_ewmh_handle_client_message(e, c)
#endif//JBWM_USE_EWMH
#endif//JBWM_EWMH_STATE_H

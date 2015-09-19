#ifndef ATOMS_H
#define ATOMS_H

#define GETATOM(atom) XInternAtom(jbwm.X.dpy, atom, False)

#define ARWM_ATOM_VWM_STATE 	GETATOM("_NET_WM_STATE")
#define ARWM_ATOM_VWM_DESKTOP	GETATOM("_NET_WM_DESKTOP")
#define ARWM_ATOM_VWM_STICKY	GETATOM("_NET_WM_STATE_STICKY")

#define ARWM_ATOM_WM_STATE	GETATOM("WM_STATE")
#define ARWM_ATOM_WM_PROTOS	GETATOM("WM_PROTOCOLS")
#define ARWM_ATOM_WM_DELETE	GETATOM("WM_DELETE_WINDOW")

#define ARWM_ATOM_MWM_HINTS	GETATOM("_XA_MWM_HINTS");

#endif /* ATOMS_H */

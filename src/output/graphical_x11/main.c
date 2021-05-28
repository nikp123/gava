#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/XKBlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <iniparser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../graphical.h"
#include "../shared/egl.h"
#include "../../config.h"
#include "../../shared.h"
#include "main.h"

#ifdef STARS
	#include <math.h>

	typedef struct star {
		float x;	// because movements are so small ints may not capture them properly
		float y;
		float angle;
		char size;
	} star;

	static star *stars;
	static double speed;
	static float kineticScore;

	static int starsCount;
	static double starsSens, starsVelocity;
#endif

// random magic values go here
#define SCREEN_CONNECTED 0

static Pixmap gradientBox = 0;
static XColor xbgcol, xcol, *xgrad;

static XEvent xavaXEvent;
static Colormap xavaXColormap;
static Display *xavaXDisplay;
static Screen *xavaXScreen;
static Window xavaXWindow, xavaXRoot;
static GC xavaXGraphics;

static XVisualInfo xavaVInfo;
static XSetWindowAttributes xavaAttr;
static Atom wm_delete_window, wmState, fullScreen, mwmHintsProperty, wmStateBelow, taskbar;
static XClassHint xavaXClassHint;
static XWMHints xavaXWMHints;
static XEvent xev;
static Bool xavaSupportsRR;
static int xavaRREventBase;

static XWindowAttributes  windowAttribs;
static XRRScreenResources *xavaXScreenResources;
static XRROutputInfo      *xavaXOutputInfo;
static XRRCrtcInfo        *xavaXCrtcInfo;

static char  *monitorName;
static bool rootWindowEnabled;
static bool overrideRedirectEnabled;
static bool reloadOnDisplayConfigure;

#ifdef EGL
	static struct _escontext ESContext;
#endif

// mwmHints helps us comunicate with the window manager
struct mwmHints {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long input_mode;
	unsigned long status;
};

static int xavaXScreenNumber;

// Some window manager definitions
enum {
	MWM_HINTS_FUNCTIONS = (1L << 0),
	MWM_HINTS_DECORATIONS =  (1L << 1),

	MWM_FUNC_ALL = (1L << 0),
	MWM_FUNC_RESIZE = (1L << 1),
	MWM_FUNC_MOVE = (1L << 2),
	MWM_FUNC_MINIMIZE = (1L << 3),
	MWM_FUNC_MAXIMIZE = (1L << 4),
	MWM_FUNC_CLOSE = (1L << 5)
};
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

// Pull from the terminal colors, and afterwards, do the usual
void snatchColor(char *name, char *colorStr, int colorNum, XColor *colorObj,
		char *databaseName, XrmDatabase *xavaXResDB) {
	XrmValue value;
	char *type;
	if(!strcmp(colorStr, "default")) {
		if(databaseName) {
			if(XrmGetResource(*xavaXResDB, name, NULL, &type, &value))
				XParseColor(xavaXDisplay, xavaXColormap, value.addr, colorObj);
		} else {
			char tempColorStr[8];
			sprintf(tempColorStr, "#%06x", colorNum);
			XParseColor(xavaXDisplay, xavaXColormap, tempColorStr, colorObj);
		}
	} else {
		char tempColorStr[8];
		sprintf(tempColorStr, "#%06x", colorNum);
		XParseColor(xavaXDisplay, xavaXColormap, tempColorStr, colorObj);
	}
	XAllocColor(xavaXDisplay, xavaXColormap, colorObj);
}

void calculateColors(struct config_params *p) {
	XrmInitialize();
	XrmDatabase xavaXResDB = NULL;
	char *databaseName = XResourceManagerString(xavaXDisplay);
	if(databaseName) {
		xavaXResDB = XrmGetStringDatabase(databaseName);
	}
	snatchColor("color5", p->color, p->col, &xcol, databaseName, &xavaXResDB);
	snatchColor("color4", p->bcolor, p->bgcol, &xbgcol, databaseName, &xavaXResDB);
}

EXP_FUNC int xavaInitOutput(struct XAVA_HANDLE *hand) {
	struct config_params *p = &hand->conf;

	// NVIDIA CPU cap utilization in Vsync fix
	setenv("__GL_YIELD", "USLEEP", 0);

	// workarounds go above if they're required to run before anything else

	// connect to the X server
	xavaXDisplay = XOpenDisplay(NULL);
	xavaBailCondition(!xavaXDisplay, "Could not find X11 display");

	xavaXScreen = DefaultScreenOfDisplay(xavaXDisplay);
	xavaXScreenNumber = DefaultScreen(xavaXDisplay);
	xavaXRoot = RootWindow(xavaXDisplay, xavaXScreenNumber);

	// select appropriate screen
	xavaXScreenResources = XRRGetScreenResources(xavaXDisplay, 
		DefaultRootWindow(xavaXDisplay));
	char *screenname = NULL; // potential bugfix if X server has no displays

	xavaSpam("Number of detected screens: %d", xavaXScreenResources->noutput);

	int screenwidth, screenheight, screenx, screeny;
	for(int i = 0; i < xavaXScreenResources->noutput; i++) {
		xavaXOutputInfo = XRRGetOutputInfo(xavaXDisplay, xavaXScreenResources,
			xavaXScreenResources->outputs[i]);

		if(xavaXOutputInfo->connection != SCREEN_CONNECTED)
			continue;

		// If the current mode is above the number of modes supported, that means
		// that THAT display is actually just disabled, so we ignore it
		if(xavaXScreenResources->ncrtc > xavaXOutputInfo->ncrtc)
			continue;

		xavaXCrtcInfo = XRRGetCrtcInfo(xavaXDisplay, xavaXScreenResources,
			xavaXOutputInfo->crtc);

		screenwidth  = xavaXCrtcInfo->width;
		screenheight = xavaXCrtcInfo->height;
		screenx      = xavaXCrtcInfo->x;
		screeny      = xavaXCrtcInfo->y;
		screenname   = strdup(xavaXOutputInfo->name);

		XRRFreeCrtcInfo(xavaXCrtcInfo);
		XRRFreeOutputInfo(xavaXOutputInfo);

		if(!strcmp(screenname, monitorName)) {
			calculate_win_pos(p, screenwidth, screenheight);

			// correct window offsets
			p->wx += screenx;
			p->wy += screeny;
			break;
		} else {
			free(screenname);
			// magic value for no screen found
			screenname = NULL;
		}
	}
	XRRFreeScreenResources(xavaXScreenResources);

	// in case that no screen matches, just use default behavior
	if(screenname == NULL) {
		calculate_win_pos(p, xavaXScreen->width, xavaXScreen->height);
	}

	// 32 bit color means alpha channel support
	XMatchVisualInfo(xavaXDisplay, xavaXScreenNumber, p->transF ? 32 : 24, TrueColor, &xavaVInfo);

	xavaAttr.colormap = XCreateColormap(xavaXDisplay, DefaultRootWindow(xavaXDisplay), xavaVInfo.visual, AllocNone);
	xavaXColormap = xavaAttr.colormap;
	calculateColors(p);
	xavaAttr.background_pixel = p->transF ? 0 : xbgcol.pixel;
	xavaAttr.border_pixel = xcol.pixel;

	xavaAttr.backing_store = Always;
	// make it so that the window CANNOT be reordered by the WM
	xavaAttr.override_redirect = overrideRedirectEnabled;

	int xavaWindowFlags = CWOverrideRedirect | CWBackingStore |  CWEventMask | CWColormap | CWBorderPixel | CWBackPixel;

	if(rootWindowEnabled) xavaXWindow = xavaXRoot;
	#ifdef STARS
	else xavaXWindow = XCreateWindow(xavaXDisplay, xavaXRoot, screenx, screeny, screenwidth,
			screenheight, 0, xavaVInfo.depth, InputOutput, xavaVInfo.visual, xavaWindowFlags, &xavaAttr);
	#else
	else xavaXWindow = XCreateWindow(xavaXDisplay, xavaXRoot, p->wx, p->wy, (unsigned int)p->w,
		(unsigned int)p->h, 0, xavaVInfo.depth, InputOutput, xavaVInfo.visual, xavaWindowFlags, &xavaAttr);
	#endif
	XStoreName(xavaXDisplay, xavaXWindow, "XAVA");

	// The "X" button is handled by the window manager and not Xorg, so we set up a Atom
	wm_delete_window = XInternAtom (xavaXDisplay, "WM_DELETE_WINDOW", 0);
	XSetWMProtocols(xavaXDisplay, xavaXWindow, &wm_delete_window, 1);

	xavaXClassHint.res_name = (char *)"XAVA";
	//xavaXWMHints.flags = InputHint | StateHint;
	//xavaXWMHints.initial_state = NormalState;
	xavaXClassHint.res_class = (char *)"XAVA";
	XmbSetWMProperties(xavaXDisplay, xavaXWindow, NULL, NULL, NULL, 0, NULL, &xavaXWMHints, &xavaXClassHint);

	XSelectInput(xavaXDisplay, xavaXWindow, RRScreenChangeNotifyMask | VisibilityChangeMask | StructureNotifyMask | ExposureMask | KeyPressMask | KeymapNotify);

	#ifdef EGL
		ESContext.native_window = xavaXWindow;
		ESContext.native_display = xavaXDisplay;
		EGLCreateContext(hand, &ESContext);
		EGLInit(hand);
	#endif

	XMapWindow(xavaXDisplay, xavaXWindow);
	xavaXGraphics = XCreateGC(xavaXDisplay, xavaXWindow, 0, 0);

	if(p->gradients) {
		xgrad = malloc((p->gradients+1)*sizeof(XColor));
		XParseColor(xavaXDisplay, xavaXColormap, p->gradient_colors[0], &xgrad[p->gradients]);
		XAllocColor(xavaXDisplay, xavaXColormap, &xgrad[p->gradients]);
		for(unsigned int i=0; i<p->gradients; i++) {
			XParseColor(xavaXDisplay, xavaXColormap, p->gradient_colors[i], &xgrad[i]);
			XAllocColor(xavaXDisplay, xavaXColormap, &xgrad[i]);
		}
	}

	// Set up atoms
	wmState = XInternAtom(xavaXDisplay, "_NET_WM_STATE", 0);
	taskbar = XInternAtom(xavaXDisplay, "_NET_WM_STATE_SKIP_TASKBAR", 0);
	fullScreen = XInternAtom(xavaXDisplay, "_NET_WM_STATE_FULLSCREEN", 0);
	mwmHintsProperty = XInternAtom(xavaXDisplay, "_MOTIF_WM_HINTS", 0);
	wmStateBelow = XInternAtom(xavaXDisplay, "_NET_WM_STATE_BELOW", 1);

	/**
	 * Set up window properties through Atoms,
	 * 
	 * XSendEvent requests the following format:
	 *  window  = the respective client window
	 *  message_type = _NET_WM_STATE
	 *  format = 32
	 *  data.l[0] = the action, as listed below
	 *  data.l[1] = first property to alter
	 *  data.l[2] = second property to alter
	 *  data.l[3] = source indication (0-unk,1-normal app,2-pager)
	 *  other data.l[] elements = 0
	**/
	xev.xclient.type = ClientMessage;
	xev.xclient.window = xavaXWindow;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	xev.xclient.data.l[2] = 0;
	xev.xclient.data.l[3] = 0;
	xev.xclient.data.l[4] = 0;

	// keep window in bottom property
	xev.xclient.data.l[0] = p->bottomF ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = wmStateBelow;
	XSendEvent(xavaXDisplay, xavaXRoot, 0, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	if(p->bottomF) XLowerWindow(xavaXDisplay, xavaXWindow);

	// remove window from taskbar
	xev.xclient.data.l[0] = p->taskbarF ? _NET_WM_STATE_REMOVE : _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = taskbar;
	XSendEvent(xavaXDisplay, xavaXRoot, 0, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	// Setting window options
	struct mwmHints hints;
	hints.flags = (1L << 1);
	hints.decorations = p->borderF;
	XChangeProperty(xavaXDisplay, xavaXWindow, mwmHintsProperty, mwmHintsProperty, 32, PropModeReplace, (unsigned char *)&hints, 5);

	// move the window in case it didn't by default
	XWindowAttributes xwa;
	XGetWindowAttributes(xavaXDisplay, xavaXWindow, &xwa);
	if(strcmp(p->winA, "none"))
	#ifdef STARS
		//XMoveWindow(xavaXDisplay, xavaXWindow, screenx, screeny);
		XMoveWindow(xavaXDisplay, xavaXWindow, 0, 0);
	#else
		XMoveWindow(xavaXDisplay, xavaXWindow, p->wx, p->wy);
	#endif

	// query for the RR extension in X11
	int error;
	xavaSupportsRR = XRRQueryExtension(xavaXDisplay, &xavaRREventBase, &error);
	if(xavaSupportsRR) {
		int rr_major, rr_minor;

		if(XRRQueryVersion(xavaXDisplay, &rr_major, &rr_minor)) {
			int rr_mask = RRScreenChangeNotifyMask;
			if(rr_major == 1 && rr_minor <= 1) {
				rr_mask &= ~(RRCrtcChangeNotifyMask |
					RROutputChangeNotifyMask |
					RROutputPropertyNotifyMask);
			}

			// listen for display configure events only if enabled
			if(reloadOnDisplayConfigure)
				XRRSelectInput(xavaXDisplay, xavaXWindow, rr_mask);
		}
	}

	#ifdef STARS
		// star count doesn't change in runtime, so allocation is only done during init
		stars = calloc(starsCount, sizeof(star));
	#endif

	return 0;
}

int render_gradient_x(struct config_params *p) {
	if(gradientBox != 0) XFreePixmap(xavaXDisplay, gradientBox);

	gradientBox = XCreatePixmap(xavaXDisplay, xavaXWindow, (unsigned int)p->bw, (unsigned int)p->h, 32);
	// TODO: Error checks

	for(unsigned int I = 0; I < (unsigned int)p->h; I++) {
		// don't touch +1.0/w_h at the end fixes some math problems
		double step = (double)(I%((unsigned int)p->h/(p->gradients-1)))/(double)((unsigned int)p->h/(p->gradients-1));

		// to future devs: this isnt ARGB. this is something internal that cannot be changed by simple means

		unsigned int gcPhase = (p->gradients-1)*I/(unsigned int)p->h;
		xgrad[p->gradients].red   = UNSIGNED_TRANS(xgrad[gcPhase].red, xgrad[gcPhase+1].red, step);
		xgrad[p->gradients].green = UNSIGNED_TRANS(xgrad[gcPhase].green, xgrad[gcPhase+1].green, step);
		xgrad[p->gradients].blue  = UNSIGNED_TRANS(xgrad[gcPhase].blue, xgrad[gcPhase+1].blue, step);
		xgrad[p->gradients].flags = DoRed | DoGreen | DoBlue;
		XAllocColor(xavaXDisplay, xavaXColormap, &xgrad[p->gradients]);

		XSetForeground(xavaXDisplay, xavaXGraphics, xgrad[p->gradients].pixel);
		XFillRectangle(xavaXDisplay, gradientBox, xavaXGraphics, 0, p->h - (int)I, (unsigned int)p->w, 1);
	}
	return 0;
}

EXP_FUNC void xavaOutputClear(struct XAVA_HANDLE *hand) {
	#if defined(EGL)
		EGLClear(hand);
	#else
		struct config_params *p = &hand->conf;

		XSetBackground(xavaXDisplay, xavaXGraphics, xbgcol.pixel);
		XClearWindow(xavaXDisplay, xavaXWindow);

		if(p->gradients) render_gradient_x(p);
	#endif
}

EXP_FUNC int xavaOutputApply(struct XAVA_HANDLE *hand) {
	struct config_params *p = &hand->conf;

	calculateColors(p);
	// Gets the monitors resolution
	if(p->fullF){
		p->w = DisplayWidth(xavaXDisplay, xavaXScreenNumber);
		p->h = DisplayHeight(xavaXDisplay, xavaXScreenNumber);
	}

	//Atom xa = XInternAtom(xavaXDisplay, "_NET_WM_WINDOW_TYPE", 0); May be used in the future
	//Atom prop;

	// change window type (this makes sure that compoziting managers don't mess with it)
	//if(xa != NULL)
	//{
	//	prop = XInternAtom(xavaXDisplay, "_NET_WM_WINDOW_TYPE_DESKTOP", 0);
	//	XChangeProperty(xavaXDisplay, xavaXWindow, xa, XA_ATOM, 32, PropModeReplace, (unsigned char *) &prop, 1);
	//}
	// The code above breaks stuff, please don't use it.	
	

	// tell the window manager to switch to a fullscreen state
	xev.xclient.type=ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = 1;
	xev.xclient.window = xavaXWindow;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = p->fullF ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = fullScreen;
	xev.xclient.data.l[2] = 0;
	XSendEvent(xavaXDisplay, xavaXRoot, 0, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	xavaOutputClear(hand);

	if(!p->interactF){
		XRectangle rect;
		XserverRegion region = XFixesCreateRegion(xavaXDisplay, &rect, 1);
		XFixesSetWindowShapeRegion(xavaXDisplay, xavaXWindow, ShapeInput, 0, 0, region);
		XFixesDestroyRegion(xavaXDisplay, region);
	}

	#ifdef STARS
		// when display is resize, all information is considered unsafe
		// which is why initial star positions are calcuated here

		int displayWidth  = DisplayWidth(xavaXDisplay, xavaXScreenNumber);
		int displayHeight = DisplayHeight(xavaXDisplay, xavaXScreenNumber);
		for(int i=0; i<starsCount; i++) {
			stars[i].x = rand()%displayWidth;
			stars[i].y = rand()%displayHeight;

			// it is absolutely critical that this angle be in range(-pi/2, +pi/2)
			// going outside of this will cause numerous bugs
			stars[i].angle = pow((float)(rand()%1200-600)/1000.0, 3.0);
			stars[i].size = (5-sqrt(rand()%26))+1;
		}
	#endif

	XGetWindowAttributes(xavaXDisplay, xavaXWindow, &windowAttribs);

	#ifdef EGL
		EGLApply(hand);
	#endif

	return 0;
}

EXP_FUNC XG_EVENT xavaOutputHandleInput(struct XAVA_HANDLE *hand) {
	struct config_params *p = &hand->conf;

	// this way we avoid event stacking which requires a full frame to process a single event
	XG_EVENT action = XAVA_IGNORE;

	while(XPending(xavaXDisplay)) {
		XNextEvent(xavaXDisplay, &xavaXEvent);
		switch(xavaXEvent.type) {
			case KeyPress:
			{
				KeySym key_symbol;
				key_symbol = XkbKeycodeToKeysym(xavaXDisplay, xavaXEvent.xkey.keycode, 0, xavaXEvent.xkey.state & ShiftMask ? 1 : 0);
				switch(key_symbol) {
					// should_reload = 1
					// resizeTerminal = 2
					// bail = -1
					case XK_a:
						p->bs++;
						return XAVA_RESIZE;
					case XK_s:
						if(p->bs > 0) p->bs--;
						return XAVA_RESIZE;
					case XK_f: // fullscreen
						p->fullF = !p->fullF;
						return XAVA_RESIZE;
					case XK_Up:
						p->sens *= 1.05;
						break;
					case XK_Down:
						p->sens *= 0.95;
						break;
					case XK_Left:
						p->bw++;
						return XAVA_RESIZE;
					case XK_Right:
						if (p->bw > 1) p->bw--;
						return XAVA_RESIZE;
					case XK_r: //reload config
						return XAVA_RELOAD;
					case XK_q:
						return XAVA_QUIT;
					case XK_Escape:
						return XAVA_QUIT;
					case XK_b:
						// WARNING: Assumes that alpha is the
						// upper 8-bits and that rand is 16-bit
						p->bgcol &= 0xff00000;
						p->bgcol |= rand()<<16|rand() & 0x00ffffff;

						xbgcol.red   = rand();
						xbgcol.green = rand();
						xbgcol.blue  = rand();
						xbgcol.flags = DoRed | DoGreen | DoBlue;
						XAllocColor(xavaXDisplay, xavaXColormap, &xbgcol);
						return XAVA_REDRAW;
					case XK_c:
						if(p->gradients) break;
						// WARNING: Assumes that alpha is the
						// upper 8-bits and that rand is 16-bit
						p->col &= 0xff00000;
						p->col |= rand()<<16|rand() & 0x00ffffff;

						xcol.red   = rand();
						xcol.green = rand();
						xcol.blue  = rand();
						xcol.flags = DoRed | DoGreen | DoBlue;
						XAllocColor(xavaXDisplay, xavaXColormap, &xcol);
						return XAVA_REDRAW;
				}
				break;
			}
			case ConfigureNotify:
			{
				// the window should not be resized when it IS the monitor
				if(rootWindowEnabled||overrideRedirectEnabled)
					break;

				// This is needed to track the window size
				XConfigureEvent trackedXavaXWindow;
				trackedXavaXWindow = xavaXEvent.xconfigure;
				if(p->w != trackedXavaXWindow.width || p->h != trackedXavaXWindow.height) {
					p->w = trackedXavaXWindow.width;
					p->h = trackedXavaXWindow.height;
				}
				action = XAVA_RESIZE;
				break;
			}
			case Expose:
				if(action != XAVA_RESIZE)
					action = XAVA_REDRAW;
				break;
			case VisibilityNotify:
				if(xavaXEvent.xvisibility.state == VisibilityUnobscured)
					action = XAVA_RESIZE;
				break;
			case ClientMessage:
				if((Atom)xavaXEvent.xclient.data.l[0] == wm_delete_window)
					return XAVA_QUIT;
				break;
			default:
				if(xavaXEvent.type == xavaRREventBase + RRScreenChangeNotify) {
					xavaLog("Display change detected - Restarting...\n");
					return XAVA_RELOAD;
				}
		}
	}
	return action;
}

EXP_FUNC void xavaOutputDraw(struct XAVA_HANDLE *hand) {
	struct config_params *p = &hand->conf;

	// im lazy, but i just wanna make it work
	int xoffset = hand->rest, yoffset = p->h;

	#ifndef STARS
	if(rootWindowEnabled)
	#endif
	{
		xoffset+=p->wx;
		yoffset+=p->wy;
	}

	#ifdef STARS
		// since it's impossible to do delta draws with stars, we'll
		// just redraw the framebuffer each time (even if that costs CPU time)
		float displayWidth  = DisplayWidth(xavaXDisplay, xavaXScreenNumber);
		float displayHeight = DisplayHeight(xavaXDisplay, xavaXScreenNumber);
		//XClearWindow(xavaXDisplay, xavaXWindow);

		// FIXME: Bugged screen coordinates on multimonitor (THIS FIX IS A MUST)
		//printf("%d %d\n", windowAttribs.width, windowAttribs.height);
		XClearArea(xavaXDisplay, xavaXWindow, 0, 0, windowAttribs.width, windowAttribs.height, 0);

		for(int i=0; i<starsCount; i++) {
			stars[i].x += cos(stars[i].angle)*stars[i].size*speed;
			stars[i].y += sin(stars[i].angle)*stars[i].size*speed;

			if(stars[i].x > displayWidth) {
				stars[i].x = 0;
				stars[i].y = fmod((float)rand(), displayHeight);
			}

			if(stars[i].y > displayHeight || stars[i].y < -stars[i].size) {
				stars[i].y = fmod((float)rand(), displayWidth);
				stars[i].y = stars[i].angle > 0.0 ? 1.0 : displayHeight-1.0;
			}

			XSetForeground(xavaXDisplay, xavaXGraphics, xcol.pixel);
			XFillRectangle(xavaXDisplay, xavaXWindow, xavaXGraphics, stars[i].x,
					stars[i].y, stars[i].size, stars[i].size);
		}

		// size the framebuffer is reset on each frame
		// last bars should always be 0
		memset(hand->fl, 0x00, sizeof(int)*hand->bars);

		kineticScore=0.0;
	#endif

	#if defined(EGL)
		EGLDraw(hand);
		eglSwapBuffers(ESContext.display, ESContext.surface); 
	#else
		// draw bars on the X11 window
		for(int i = 0; i < hand->bars; i++) {
			// this fixes a rendering bug
			if(hand->f[i] > p->h) hand->f[i] = p->h;

			#ifdef STARS
				// kinetic score has a low-freq bias as they are more "physical"
				float bar_percentage = (float)(hand->f[i]-1)/(float)p->h;
				if(bar_percentage > 0.0) {
					kineticScore+=pow(bar_percentage, 2.0*(float)i/(float)hand->bars);
				}
			#endif

			if(hand->f[i] > hand->fl[i]) {
				if(p->gradients)
					XCopyArea(xavaXDisplay, gradientBox, xavaXWindow, xavaXGraphics, 
						0, p->h - hand->f[i], p->bw, hand->f[i]-hand->fl[i],
						xoffset + i*(p->bs+p->bw), yoffset - hand->f[i]);
				else {
					XSetForeground(xavaXDisplay, xavaXGraphics, xcol.pixel);
					XFillRectangle(xavaXDisplay, xavaXWindow, xavaXGraphics,
						xoffset + i*(p->bs+p->bw), yoffset - hand->f[i], 
						p->bw, hand->f[i]-hand->fl[i]);
				}
			} else if (hand->f[i] < hand->fl[i]) {
				XClearArea(xavaXDisplay, xavaXWindow,
					xoffset + i*(p->bs+p->bw), yoffset - hand->fl[i], 
					p->bw, hand->fl[i]-hand->f[i], 0);
			}
		}
		XSync(xavaXDisplay, 0);
	#endif

	#ifdef STARS
		speed = pow(kineticScore/(float)hand->bars, 1.0/starsSens)*starsVelocity;
	#endif
	return;
}

EXP_FUNC void xavaOutputCleanup(struct XAVA_HANDLE *hand) {
	// Root mode leaves artifacts on screen even though the window is dead
	XClearWindow(xavaXDisplay, xavaXWindow);

	// make sure that all events are dead by this point
	XSync(xavaXDisplay, 1);

	#ifdef EGL
		EGLClean(&ESContext);
	#endif

	if(gradientBox != 0) { XFreePixmap(xavaXDisplay, gradientBox); gradientBox = 0; };

	#ifdef STARS
		free(stars);
	#endif

	XFreeGC(xavaXDisplay, xavaXGraphics);
	XDestroyWindow(xavaXDisplay, xavaXWindow);
	XFreeColormap(xavaXDisplay, xavaXColormap);
	XCloseDisplay(xavaXDisplay);
	free(xgrad);
	free(monitorName);
	return;
}

EXP_FUNC void xavaOutputHandleConfiguration(struct XAVA_HANDLE *hand, void *data) {
	struct config_params *p = &hand->conf;

	dictionary *ini = (dictionary*)data;

	reloadOnDisplayConfigure = iniparser_getboolean
		(ini, "x11:reload_on_display_configure", 0);
	rootWindowEnabled = iniparser_getboolean
		(ini, "x11:root_window", 0);
	overrideRedirectEnabled = iniparser_getboolean
		(ini, "x11:override_redirect", 0);
	monitorName = strdup(iniparser_getstring
		(ini, "x11:monitor_name", "none"));

	// who knew that messing with some random properties breaks things
	xavaWarnCondition(rootWindowEnabled&&overrideRedirectEnabled,
			"'root_window' and 'override_redirect' don't work together!");

	xavaBailCondition(rootWindowEnabled&&p->gradients,
			"'root_window' and gradients don't work together!");

	// Xquartz doesnt support ARGB windows
	// Therefore transparency is impossible on macOS
	#ifdef __APPLE__
		p->transF = 0;
	#endif

	#ifndef EGL
		// VSync doesnt work without OpenGL :(
		p->vsync = 0;
	#else
		// OGL + Root Window = completely broken
		rootWindowEnabled = 0;
	#endif

	#ifdef STARS
		starsCount = iniparser_getint(ini,   "stars:count", 100);
		xavaBailCondition(starsCount < 1, "Stars do not work if their number is below 1");

		starsSens = iniparser_getdouble(ini, "stars:sensitivity", 1.2);
		xavaBailCondition(starsSens<=0.0, "Stars sensitivity needs to be above 0.0");

		starsVelocity = 2.0*iniparser_getdouble(ini, "stars:velocity", 1.0);
		xavaBailCondition(starsVelocity<=0.0, "Stars velocity needs to be above 0.0");
	#endif

	#ifdef EGL
		EGLShadersLoad();
	#endif
}


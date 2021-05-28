#include <GLES2/gl2.h>
#include <stdint.h>
#include <wayland-egl-core.h>
#include <wayland-egl.h>
#include <EGL/egl.h>

// this is so that my static analyser doesn't bitch as much
#ifndef EGL
	#define EGL
#endif

#include "../graphical.h"
#include "../shared/egl.h"

#include "egl.h"
#include "main.h"
#include "render.h"

struct _escontext ESContext;

// dummy abstraction function
void waylandEGLShadersLoad(void) {
	EGLShadersLoad();
}

void waylandEGLCreateWindow(struct waydata *wd) {
	//region = wl_compositor_create_region(wd->compositor);
	//wl_region_add(region, 0, 0, width, height);
	//wl_surface_set_opaque_region(surface, region);

	struct wl_egl_window *egl_window = wl_egl_window_create(wd->surface,
			wd->hand->conf.w, wd->hand->conf.h);

	xavaBailCondition(egl_window == EGL_NO_SURFACE,
			"Failed to create EGL window!\n");

	xavaSpam("Created EGL window!");

	ESContext.native_window = egl_window;
	ESContext.native_display = wd->display;
}

void waylandEGLInit(struct waydata *wd) {
	struct XAVA_HANDLE *xava = wd->hand;

	// creates everything EGL related
	waylandEGLCreateWindow(wd);

	xavaBailCondition(EGLCreateContext(xava, &ESContext) == EGL_FALSE,
			"Failed to create EGL context");

	EGLInit(xava);
}

void waylandEGLWindowResize(struct waydata *wd, int w, int h) {
	wl_egl_window_resize(ESContext.native_window, w, h, 0, 0);
	wl_surface_commit(wd->surface);
}

void waylandEGLApply(struct XAVA_HANDLE *xava) {
	EGLApply(xava);
}

void waylandEGLDraw(struct XAVA_HANDLE *xava) {
	EGLDraw(xava);
	eglSwapBuffers(ESContext.display, ESContext.surface); 
}

void waylandEGLDestroy(struct waydata *wd) {
	EGLClean(&ESContext);
	wl_egl_window_destroy(ESContext.native_window);
}


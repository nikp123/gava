#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

struct _escontext {
	// because windowing systems are complicated
	EGLNativeDisplayType native_display;
	EGLNativeWindowType native_window;

	// EGL display
	EGLDisplay  display;
	// EGL context
	EGLContext  context;
	// EGL surface
	EGLSurface  surface;
};

void       EGLShadersLoad(void);
void       EGLInit(struct XAVA_HANDLE *xava);
void       EGLApply(struct XAVA_HANDLE *xava);
void       EGLDraw(struct XAVA_HANDLE *xava);
EGLBoolean EGLCreateContext(struct XAVA_HANDLE *xava, struct _escontext *ESContext);
void       EGLClean(struct _escontext *ESContext);


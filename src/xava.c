#include "log.h"
#define TRUE 1
#define FALSE 0

#define _XOPEN_SOURCE_EXTENDED
#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#if defined(__unix__)||defined(__APPLE__)
	#include <termios.h>
#endif
#include <math.h>
#include <fcntl.h> 

#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>

#if defined(__linux__)||defined(WIN)
	#include "misc/inode_watcher.h"
#endif

#ifndef PACKAGE
	#define PACKAGE "INSERT_PROGRAM_NAME"
	#warning "Package name not defined!"
#endif

#ifndef VERSION
	#define VERSION "INSERT_PROGRAM_VERSION"
	#warning "Package version not defined!"
#endif

#include "output/graphical.h"
#include "input/fifo/main.h"
#include "config.h"
#include "shared.h"

static void*    (*xavaInput)                     (void*); // technically it's "struct audio_data*"
                                                          // but the compiler complains :(
static void     (*xavaInputHandleConfiguration)  (struct audio_data*, void*);

static void     (*xavaOutputHandleConfiguration) (struct XAVA_HANDLE*, void*);
static int      (*xavaInitOutput)                (struct XAVA_HANDLE*);
static void     (*xavaOutputClear)               (struct XAVA_HANDLE*);
static int      (*xavaOutputApply)               (struct XAVA_HANDLE*);
static XG_EVENT (*xavaOutputHandleInput)         (struct XAVA_HANDLE*);
static void     (*xavaOutputDraw)                (struct XAVA_HANDLE*);
static void     (*xavaOutputCleanup)             (struct XAVA_HANDLE*);

static void     (*xavaFilterHandleConfiguration) (struct XAVA_HANDLE*, void*);
static int      (*xavaFilterInit)                (struct XAVA_HANDLE*);
static int      (*xavaFilterApply)               (struct XAVA_HANDLE*);
static int      (*xavaFilterLoop)                (struct XAVA_HANDLE*);
static int      (*xavaFilterCleanup)             (struct XAVA_HANDLE*);


static _Bool kys = 0, should_reload = 0;

// teh main XAVA handle (made just to not piss off MinGW)
static struct XAVA_HANDLE xava;

// XAVA magic variables, too many of them indeed
static pthread_t p_thread;

// general: cleanup
void cleanup(void) {
	struct config_params *p     = &xava.conf;
	struct audio_data    *audio = &xava.audio;

	#if defined(__linux__)||defined(__WIN32__)
		// we need to do this since the inode watcher is a seperate thread
		destroyFileWatcher();
	#endif

	// telling audio thread to terminate 
	audio->terminate = 1;

	// waiting for all background threads and other stuff to terminate properly
	xavaSleep(100, 0);

	// kill the audio thread
	pthread_join(p_thread, NULL);

	xavaOutputCleanup(&xava);
	xavaFilterCleanup(&xava);

	// destroy modules
	destroy_module(p->inputModule);
	destroy_module(p->outputModule);
	destroy_module(p->filterModule);

	// clean up XAVA internal variables
	free(audio->source);

	// cleanup remaining FFT buffers (abusing C here)
	switch(audio->channels) {
		case 2:
			free(audio->audio_out_r);
		default:
			free(audio->audio_out_l);
			break;
	}

	// clean the config
	clean_config();
}

#if defined(__unix__)||defined(__APPLE__)
// general: handle signals
void sig_handler(int sig_no) {
	switch(sig_no) {
		case SIGUSR1:
			should_reload = true;
			break;
		case SIGINT:
			xavaLog("CTRL-C pressed -- goodbye\n");
			kys=1;
			return;
		case SIGTERM:
			xavaLog("Process termination requested -- goodbye\n");
			kys=1;
			return;
	}
}
#endif

// general: entry point
int main(int argc, char **argv)
{
	// general: define variables
	//int thr_id;

	int sleep = 0;
	int i, c, silence;
	char configPath[255];
	char *usage = "\n\
Usage : " PACKAGE " [options]\n\
Visualize audio input in a window. \n\
\n\
Options:\n\
    -p          specify path to the config file\n\
    -v          print version\n\
\n\
Keys:\n\
        Up        Increase sensitivity\n\
        Down      Decrease sensitivity\n\
        Left      Decrease number of bars\n\
        Right     Increase number of bars\n\
        r         Reload config\n\
        c         Cycle foreground color\n\
        b         Cycle background color\n\
        q         Quit\n\
\n\
as of 0.4.0 all options are specified in config file, see in '/home/username/.config/xava/' \n";

	unsigned long oldTime = 0;

	configPath[0] = '\0';

	setlocale(LC_ALL, "C");
	setbuf(stdout,NULL);
	setbuf(stderr,NULL);

	// general: handle Ctrl+C
	#if defined(__unix__)||defined(__APPLE__)
		struct sigaction action;
		memset(&action, 0, sizeof(action));
		action.sa_handler = &sig_handler;
		sigaction(SIGINT, &action, NULL);
		sigaction(SIGTERM, &action, NULL);
		sigaction(SIGUSR1, &action, NULL);
	#endif

	// general: handle command-line arguments
	while ((c = getopt (argc, argv, "p:vh")) != -1) {
		switch (c) {
			case 'p': // argument: fifo path
				snprintf(configPath, sizeof(configPath), "%s", optarg);
				break;
			case 'h': // argument: print usage
				printf ("%s", usage);
				return 1;
			case '?': // argument: print usage
				printf ("%s", usage);
				return 1;
			case 'v': // argument: print version
				printf (PACKAGE " " VERSION "\n");
				return 0;
			default:  // argument: no arguments; exit
				abort ();
		}
	}

	// general: main loop
	while (1) {
		// extract the shorthand sub-handles
		struct config_params     *p     = &xava.conf;
		struct audio_data        *audio = &xava.audio;

		// load config
		load_config(configPath, &xava);

		// load symbols
		xavaInput                    = get_symbol_address(p->inputModule, "xavaInput");
		xavaInputHandleConfiguration = get_symbol_address(p->inputModule, "xavaInputHandleConfiguration");

		xavaInitOutput                = get_symbol_address(p->outputModule, "xavaInitOutput");
		xavaOutputClear               = get_symbol_address(p->outputModule, "xavaOutputClear");
		xavaOutputApply               = get_symbol_address(p->outputModule, "xavaOutputApply");
		xavaOutputHandleInput         = get_symbol_address(p->outputModule, "xavaOutputHandleInput");
		xavaOutputDraw                = get_symbol_address(p->outputModule, "xavaOutputDraw");
		xavaOutputCleanup             = get_symbol_address(p->outputModule, "xavaOutputCleanup");
		xavaOutputHandleConfiguration = get_symbol_address(p->outputModule, "xavaOutputHandleConfiguration");

		xavaFilterInit                = get_symbol_address(p->filterModule, "xavaFilterInit");
		xavaFilterApply               = get_symbol_address(p->filterModule, "xavaFilterApply");
		xavaFilterLoop                = get_symbol_address(p->filterModule, "xavaFilterLoop");
		xavaFilterCleanup             = get_symbol_address(p->filterModule, "xavaFilterCleanup");
		xavaFilterHandleConfiguration = get_symbol_address(p->filterModule, "xavaFilterHandleConfiguration");

		// load input config
		xavaInputHandleConfiguration((void*)get_config_pointer(), (void*)audio);

		// load output config
		xavaOutputHandleConfiguration(&xava, (void*)get_config_pointer());

		// load filter config
		xavaFilterHandleConfiguration(&xava, (void*)get_config_pointer());

		audio->inputsize = p->inputsize;
		audio->fftsize   = p->fftsize;
		MALLOC_SELF(audio->audio_out_l, p->fftsize+1);
		if(p->stereo)
			MALLOC_SELF(audio->audio_out_r, p->fftsize+1);
		audio->format = -1;
		audio->terminate = 0;
		audio->channels = 1+p->stereo;

		if(p->stereo) {
			for (i = 0; i < audio->fftsize; i++) {
				audio->audio_out_l[i] = 0;
				audio->audio_out_r[i] = 0;
			}
		} else for(i=0; i < audio->fftsize; i++) audio->audio_out_l[i] = 0;

		// thr_id = below
		pthread_create(&p_thread, NULL, xavaInput, (void*)audio);

		bool reloadConf = false;

		xavaBailCondition(xavaFilterInit(&xava),
				"Failed to initialize filter! Bailing...");

		xavaBailCondition(xavaInitOutput(&xava), 
				"Failed to initialize output! Bailing...");

		while(!reloadConf) { //jumbing back to this loop means that you resized the screen
			// handle for user setting too many bars
			if (p->fixedbars) {
				p->autobars = 0;
				if (p->fixedbars * p->bw + p->fixedbars * p->bs - p->bs > p->w) p->autobars = 1;
			}

			//getting orignial numbers of barss incase of resize
			if (p->autobars == 1)  {
				xava.bars = (p->w + p->bs) / (p->bw + p->bs);

				//if (p->bs != 0) bars = (w - bars * p->bs + p->bs) / bw;
			} else xava.bars = p->fixedbars;

			if (xava.bars < 1) xava.bars = 1; // must have at least 1 bars

			if (p->stereo) { // stereo must have even numbers of bars
				if (xava.bars%2) xava.bars--;
			}

			xavaFilterApply(&xava);

			xavaOutputApply(&xava);

			bool resizeWindow = false;
			bool redrawWindow = false;

			while  (!resizeWindow) {
				switch(xavaOutputHandleInput(&xava)) {
					case XAVA_QUIT:
						cleanup();
						return EXIT_SUCCESS;
					case XAVA_RELOAD:
						should_reload = 1;
						break;
					case XAVA_RESIZE:
						resizeWindow = TRUE;
						break;
					case XAVA_REDRAW:
						redrawWindow = TRUE;
						break;
					default:
						// ignore other values
						break;
				}
				#if defined(__linux__)||defined(__WIN32__)
					// check for updates in the config file
					if(!should_reload)
						should_reload = getFileStatus();
				#endif

				if (should_reload) {
					reloadConf = true;
					resizeWindow = true;
					should_reload = 0;
				}

				//if (cont == 0) break;

				// process: populate input buffer and check if input is present
				silence = 1;

				for (i = 0; i < audio->fftsize; i++) {
					if(audio->audio_out_l[i]) {
						silence = 0;
						break;
					}
				}

				if(p->stereo) {
					for (i = 0; i < audio->fftsize; i++) {
						if(audio->audio_out_r[i]) {
							silence = 0;
							break;
						}
					}
				}

				if (silence == 1) {
					sleep++;
				} else {
					sleep = 0;
					xavaSpamCondition(xava.pauseRendering, "Resuming from sleep");
				}

				// process: if input was present for the last 5 seconds apply FFT to it
				if (sleep < p->framerate * 5) {
					xava.pauseRendering = false;
				} else if(xava.pauseRendering) {
					// unless the user requested that the program ends
					if(kys||should_reload) sleep = 0;

					// wait 100ms, then check sound again.
					xavaSleep(100, 0);
					continue;
				} else { // if in sleep mode wait and continue
					xavaSpam("Going to sleep!");

					// signal to any potential rendering threads to stop
					xava.pauseRendering = true;
				}

				// Calculate the result through filters
				xavaFilterLoop(&xava);

				// zero values causes divided by zero segfault
				// and set max height
				int max_height = p->h - p->shdw*2;
				for (i = 0; i < xava.bars; i++) {
					if(xava.f[i] < 1) 
						xava.f[i] = 1;
					else if(xava.f[i] > max_height)
						xava.f[i] = max_height;
					xava.f[i] += p->shdw;
				}

				// output: draw processed input
				if(redrawWindow) {
					xavaOutputClear(&xava);
					memset(xava.fl, 0x00, sizeof(int)*xava.bars);
					redrawWindow = FALSE;
				}
				xavaOutputDraw(&xava);

				if(!p->vsync) // the window handles frametimes instead of XAVA
					oldTime = xavaSleep(oldTime, p->framerate);

				// save previous bar values
				memcpy(xava.fl, xava.f, sizeof(int)*xava.bars);

				if(kys) {
					resizeWindow=1;
					reloadConf=1;
				}

				// checking if audio thread has exited unexpectedly
				xavaBailCondition(audio->terminate,
						"Audio thread has exited unexpectedly.\nReason: %s", 
						audio->error_message);
			} // resize window
		} // reloading config

		// get rid of everything else  
		cleanup();

		// since this is an infinite loop we need to break out of it
		if(kys) break;
	}
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

#include "log.h"
#include "shared.h"

#ifdef __WIN32__
#include <windows.h>
#endif

#ifdef __linux__
	// I've gotten this from a stranger on the internet
	#include <linux/limits.h>
	#define MAX_PATH PATH_MAX
#endif

#ifdef __APPLE__
	#include <sys/syslimits.h>
	#define MAX_PATH PATH_MAX 
#endif

#if defined(__unix__)||defined(__APPLE__)
	#define mkdir(dir) mkdir(dir, 0770)
#else
	#define mkdir(dir) mkdir(dir)
#endif

EXP_FUNC int xavaMkdir(const char *dir) {
	/* Stolen from: https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
	/* Adapted from http://stackoverflow.com/a/2336245/119527 */
	const size_t len = strlen(dir);
	char _path[MAX_PATH];
	char *p;

	errno = 0;

	/* Copy string so its mutable */
	if (len > sizeof(_path)-1) {
		errno = ENAMETOOLONG;
		return -1;
	}
	strcpy(_path, dir);

	/* Iterate the string */
	for (p = _path + 1; *p; p++) {
		if (*p == '/') {
			/* Temporarily truncate */
			*p = '\0';

			if (mkdir(_path) != 0) {
				if (errno != EEXIST)
					return -1;
			}

			*p = '/';
		}
	}

	if (mkdir(_path) != 0) {
		if (errno != EEXIST)
			return -1;
	}
	return 0;
}

EXP_FUNC unsigned long xavaSleep(unsigned long oldTime, int framerate) {
	unsigned long newTime = 0;
	if(framerate) {
	#ifdef WIN
		SYSTEMTIME time;
		GetSystemTime(&time);
		newTime = time.wSecond*1000+time.wMilliseconds;
		if(newTime-oldTime<1000/framerate&&newTime>oldTime)
			Sleep(1000/framerate-(newTime-oldTime));
		GetSystemTime(&time);
		return time.wSecond*1000+time.wMilliseconds;
	#else
		struct timeval tv;
		gettimeofday(&tv, NULL);
		newTime = tv.tv_sec*1000+tv.tv_usec/1000;
		if(oldTime+1000/framerate>newTime)
			usleep((1000/framerate+oldTime-newTime)*1000);
		gettimeofday(&tv, NULL);
		return tv.tv_sec*1000+tv.tv_usec/1000;
	#endif
	}
	#ifdef WIN
	Sleep(oldTime);
	#else
	usleep(oldTime*1000);
	#endif
	return 0;
}


// XAVA event stack
EXP_FUNC void pushXAVAEventStack(XG_EVENT_STACK *stack, XG_EVENT event) {
	xavaSpam("XAVA event %d added (id=%d)", stack->pendingEvents, event);

	stack->pendingEvents++;

	XG_EVENT *newStack = realloc(stack->events, stack->pendingEvents*sizeof(XG_EVENT));
	stack->events = newStack;

	stack->events[stack->pendingEvents-1] = event;
}

EXP_FUNC XG_EVENT popXAVAEventStack(XG_EVENT_STACK *stack) {
	XG_EVENT *newStack;
	XG_EVENT event = stack->events[0];

	stack->pendingEvents--;
	for(int i = 0; i<stack->pendingEvents; i++) {
		stack->events[i] = stack->events[i+1];
	}

	newStack = realloc(stack->events, MIN(stack->pendingEvents, 1)*sizeof(XG_EVENT));
	stack->events = newStack;

	xavaSpam("XAVA event %d destroyed (id=%d)", stack->pendingEvents, event);

	return event;
}

EXP_FUNC XG_EVENT_STACK *newXAVAEventStack() {
	XG_EVENT_STACK *stack = calloc(1, sizeof(XG_EVENT_STACK));
	stack->pendingEvents = 0;
	stack->events = malloc(1); // needs a valid pointer here
	return stack;
}

EXP_FUNC void destroyXAVAEventStack(XG_EVENT_STACK *stack) {
	free(stack->events);
	free(stack);
}

EXP_FUNC bool pendingXAVAEventStack(XG_EVENT_STACK *stack) {
	return (stack->pendingEvents > 0) ? true : false;
}

// used for blocking in case an processing an event at the wrong
// time can cause disaster
EXP_FUNC bool isEventPendingXAVA(XG_EVENT_STACK *stack, XG_EVENT event) {
	for(int i = 0; i < stack->pendingEvents; i++) {
		if(stack->events[i] == event) return true;
	}

	return false;
}

// this function is an abstraction for checking whether or not a certain file is usable
// You define what kind of behaviour a certain file should have, then it's filename
// inside of the "file container" (for configs that would be ~/.config/xava/...) and
// actualPath (if successful) will return the path where that file could be found
// (this string is heap allocated, so a free is a must)
EXP_FUNC bool xavaFindAndCheckFile(XF_TYPE type, const char *filename, char **actualPath) {
	bool writeCheck = false;
	switch(type) {
		case XAVA_FILE_TYPE_CACHE:
			writeCheck = true;

			// TODO: When you use it, FINISH IT!
			{
				xavaError("XAVA_FILE_TYPE_CACHE is not implemented yet!");
				return false;
			}
			break;
		case XAVA_FILE_TYPE_CONFIG:
		{
			#if defined(__unix__) // FOSS-y/Linux-y implementation
				char *configHome = getenv("XDG_CONFIG_HOME");

				if(configHome == NULL) {
					xavaWarn("The current system does not support XDG_CONFIG_HOME.\n"
							"There is a high likelyhood that something may be broken");

					char *homeDir;
					xavaErrorCondition((homeDir = getenv("HOME")) == NULL,
							"This system is $HOME-less. Aborting execution...");

					// fuck strings, REEEEEEEEEEEEEEEEEEe
					configHome = malloc(MAX_PATH);

					if(configHome == NULL) {
						xavaError("Allocation failed");
						return false;
					}

					// filename is added in post
					sprintf(configHome, "%s/.config/%s/", homeDir, PACKAGE); 
					(*actualPath) = configHome;
				} else {
					(*actualPath) = malloc((strlen(configHome)+strlen(PACKAGE)+strlen(filename)+3)*sizeof(char));

					if((*actualPath) == NULL) {
						xavaError("Allocation failed");
						return false;
					}

					sprintf((*actualPath), "%s/%s/", configHome, PACKAGE);
				}
			#elif defined(__APPLE__) // App-lol implementation
				char *configHome = malloc(MAX_PATH);
				char *homeDir;

				// you must have a really broken system for this to be false
				if((homeDir = getenv("HOME")) == NULL) {
					xavaError("macOS is $HOME-less. Bailing out!!!");
					free(configHome);
					return false;
				}

				sprintf(configHome, "%s//Library//Application Support//", homeDir);
				(*actualPath) = configHome;
			#elif defined(__WIN32__) // Windows-y implementation
				char *configHome = malloc(MAX_PATH);
				char *homeDir;

				// stop using windows 98 ffs
				if((homeDir = getenv("APPDATA")) == NULL) {
					xavaError("XAVA could not find %AppData%! Something's REALLY wrong.");
					free(configHome);
					return false;
				}

				sprintf(configHome, "%s\\%s\\", homeDir, PACKAGE);
				(*actualPath) = configHome;
			#else
				#error "Platform not supported"
			#endif

			writeCheck = true;
			break;
		}
		case XAVA_FILE_TYPE_PACKAGE:
		{
			#if defined(__APPLE__)||defined(__unix__)
				// TODO: Support non-installed configurations
				char *path = malloc(MAX_PATH);
				strcpy(path, PREFIX"/share/"PACKAGE"/");
				(*actualPath) = path;
			#elif defined(__WIN32__)
				// Windows uses widechars internally
				WCHAR wpath[MAX_PATH];
				char *path = malloc(MAX_PATH);

				// Get path of where the executable is installed
				HMODULE hModule = GetModuleHandleW(NULL);
				GetModuleFileNameW(hModule, wpath, MAX_PATH);
				wcstombs(path, wpath, MAX_PATH);

				// bullshit string hacks with nikp123 (ep. 1)
				for(int i = strlen(path)-1; i > 0; i--) { // go from end of the string
					if(path[i] == '\\') { // if we've hit a path, change the characters in front
						path[i+1] = '\0'; // end the string after the path
					}
				}
			#else
				#error "Platform not supported"
			#endif
			writeCheck = false;
			break;
		}
		case XAVA_FILE_TYPE_CUSTOM_READ:
			writeCheck = false;
			break;
		case XAVA_FILE_TYPE_CUSTOM_WRITE:
			writeCheck = true;
			break;
		default:
		case XAVA_FILE_TYPE_NONE:
			xavaBail("Your code broke. The file type is invalid");
			break;
	}

	// this special little routine copies the path from the filename (if there is any)
	size_t last_dir_offset = 0;
	size_t path_size = strlen((*actualPath)); // DO NOT USE THIS LATER ON IN THE CODE
	const char *new_filename = filename;
	for(int i=0; i<strlen(filename); i++) {
		// caught a directory
		if(filename[i] == '/') {
			for(int j=last_dir_offset; j<i; j++) {
				(*actualPath)[path_size+j] = filename[j]; 
			}
			#if defined(__APPLE__) || defined(__unix__)
				(*actualPath)[path_size+i] = '/';
			#elif defined(__WIN32__)
				(*actualPath)[path_size+i] = '\\';
			#else
				#error "Platform not supported"
			#endif

			(*actualPath)[path_size+i+1] = '\0'; // because C

			last_dir_offset = i+1;
			new_filename = &filename[last_dir_offset];
		}
	}

	// config: create directory
	if(writeCheck)
		xavaMkdir((*actualPath));

	// add filename
	switch(type) {
		case XAVA_FILE_TYPE_PACKAGE:
		case XAVA_FILE_TYPE_CONFIG:
		case XAVA_FILE_TYPE_CACHE:
			strcat((*actualPath), new_filename);
			break;
		default:
			(*actualPath) = (char*)new_filename;
			break;
	}

	switch(type) {
		case XAVA_FILE_TYPE_CONFIG:
		{
			// don't be surprised if you find a lot of bugs here, beware!

			FILE *fp = fopen((*actualPath), "r"), *fn;
			if (!fp) {
				xavaWarn("File '%s' does not exist!\nTrying to make a new one...", (*actualPath));

				// if that particular config file does not exist, try copying the default one
				char *found;

				// use old filename, because it carries the sub-directories with it
				char *defaultConfigFileName = malloc(strlen(filename)+1+strlen(".example"));
				sprintf(defaultConfigFileName, "%s.example", filename);

				if(xavaFindAndCheckFile(XAVA_FILE_TYPE_PACKAGE, defaultConfigFileName, &found) == false) {
					xavaError("Could not find default config file! Bailing out...");
					free((*actualPath));
					return false;
				}

				free(defaultConfigFileName);

				// try to save the default file
				fp = fopen((*actualPath), "w");
				fn = fopen(found, "r"); // don't bother checking, it'll succeed anyway

				if(fp==NULL) {
					xavaError("Failed to save the default config file '%s' to '%s'!",
							found, (*actualPath));
					free(found);
					free((*actualPath));
					fclose(fn);
					return false;
				}

				size_t filesize;

				// jump to end of file
				fseek(fn, 0, SEEK_END);
				// report offset == filesize
				filesize = ftell(fn);
				// jump back to beginning
				fseek(fn, 0, SEEK_SET);

				// allocate buffer
				void *fileBuffer = malloc(filesize);
				if(fileBuffer == NULL) {
					xavaError("Could not allocate config file!");
					fclose(fn);
					fclose(fp);
					free(found);
					free((*actualPath));
					return false;
				}

				// read into buffer
				fread(fileBuffer, filesize, 1, fn);
				// write out that buffer
				fwrite(fileBuffer, filesize, 1, fp);
				// free the buffer
				free(fileBuffer);
				// close handles
				fclose(fn);
				fclose(fp);
				// close the default location string
				free(found);

				xavaLog("Successfully created default config file on '%s'!", (*actualPath));
			}

			// we can finally say that this has finished
			return true;
			break; // this break doesn't do shit apart from shutting up the static analyser
		}
		default:
		{
			FILE *fp;
			if(writeCheck) {
				fp = fopen((*actualPath), "w");
				if(fp == NULL) {
					xavaError("Could not open '%s' for writing!", (*actualPath));
					switch(type) {
						case XAVA_FILE_TYPE_PACKAGE:
						case XAVA_FILE_TYPE_CACHE:
							free((*actualPath));
							break;
						default:
							break;
					}
					return false;
				}
			} else {
				fp = fopen((*actualPath), "r");
				if(fp == NULL) {
					xavaError("Could not open '%s' for reading!", (*actualPath));
					switch(type) {
						case XAVA_FILE_TYPE_PACKAGE:
						case XAVA_FILE_TYPE_CACHE:
							free((*actualPath));
							break;
						default:
							break;
					}
					return false;
				}
			}
			fclose(fp);
			return true;
			break; // this also never gets executed sadly
		}
	}

	return false; // if you somehow ended up here, it's probably something breaking so "failure" it is
}

EXP_FUNC RawData *xavaReadFile(const char *file) {
	RawData *data = malloc(sizeof(RawData));

	FILE *fp = fopen(file, "r");
	if(fp == NULL) {
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	data->size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	data->data = malloc(data->size+1);
	if(data->data == NULL) {
		fclose(fp);
		return NULL;
	}

	fread(data->data, sizeof(char), data->size, fp);

	// pro-gamer move (NOTE: might not work if char is bigger than 8 bits)
	((char*)data->data)[data->size] = 0x00;

	fclose(fp);

	return data;
}

EXP_FUNC void xavaCloseFile(RawData *file) {
	free(file->data);
	free(file);
}

EXP_FUNC void *xavaDuplicateMemory(void *memory, size_t size) {
	void *duplicate = malloc(size);
	memcpy(duplicate, memory, size);
	return duplicate;
}


#ifndef __XAVA_LOG_H
#define __XAVA_LOG_H

// static analyser, please shut the fuck up
#ifdef SOURCE_PATH_SIZE
	#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#else
	#define __FILENAME__ __FILE__
#endif

extern void __internal_xavaSpam (const char *func, const char *file, int line, const char *fmt, ...);
extern void __internal_xavaLog  (const char *func, const char *file, int line, const char *fmt, ...);
extern void __internal_xavaWarn (const char *func, const char *file, int line, const char *fmt, ...);
extern void __internal_xavaError(const char *func, const char *file, int line, const char *fmt, ...);
extern void __internal_xavaDie  (void);

#ifdef DEBUG
	#define xavaSpam(fmt, ...)  __internal_xavaSpam (__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__);
	#define xavaLog(fmt, ...)   __internal_xavaLog  (__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__);
#else
	#define xavaSpam(fmt, ...)  /** nothing lol **/
	#define xavaLog(fmt, ...)   /** nothing lol **/
#endif

#define xavaWarn(fmt, ...)  __internal_xavaWarn (__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__);
#define xavaError(fmt, ...) __internal_xavaError(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__);
#define xavaBail(fmt, ...) { \
	__internal_xavaError(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); \
	__internal_xavaDie(); \
}

#define xavaSpamCondition(condition, fmt, ...) \
	if((condition)) { __internal_xavaSpam(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); }
#define xavaLogCondition(condition, fmt, ...) \
	if((condition)) { __internal_xavaLog(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); }
#define xavaWarnCondition(condition, fmt, ...) \
	if((condition)) { __internal_xavaWarn(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); }
#define xavaErrorCondition(condition, fmt, ...) \
	if((condition)) { __internal_xavaError(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); }
#define xavaBailCondition(condition, fmt, ...) { \
	if((condition)) { \
		__internal_xavaError(__func__, __FILENAME__, __LINE__, fmt, ## __VA_ARGS__); \
		__internal_xavaDie(); \
	} \
}

#endif


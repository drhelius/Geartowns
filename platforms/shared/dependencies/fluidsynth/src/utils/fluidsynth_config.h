#ifndef FLUIDSYNTH_CONFIG_H
#define FLUIDSYNTH_CONFIG_H

#define OSAL_embedded 1
#define WITH_FLOAT 1
#define LIBSNDFILE_SUPPORT 1

#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_STRING_H 1
#define HAVE_STDINT_H 1
#define HAVE_MATH_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDARG_H 1
#define HAVE_ERRNO_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1

#if defined(_WIN32)
#define HAVE_WINDOWS_H 1
#define HAVE_IO_H 1
#else
#define HAVE_UNISTD_H 1
#endif

#define HAVE_SINF 1
#define HAVE_COSF 1
#define HAVE_FABSF 1
#define HAVE_POWF 1
#define HAVE_SQRTF 1
#define HAVE_LOGF 1

#if !defined(_MSC_VER)
#define SUPPORTS_VLA 1
#endif

#endif

#ifndef ZNC_VERSION_H
#define ZNC_VERSION_H

#ifndef BUILD_WITH_CMAKE
// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR 1
#define VERSION_MINOR 7
#define VERSION_PATCH -1
// This one is for display purpose and to check ABI compatibility of modules
#define VERSION_STR "1.7.x"
#endif

// Don't use this one
#define VERSION (VERSION_MAJOR + VERSION_MINOR / 10.0)

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
#ifndef VERSION_EXTRA
#define VERSION_EXTRA ""
#endif
extern const char* ZNC_VERSION_EXTRA;

// Compilation options which affect ABI

#ifdef HAVE_IPV6
#define ZNC_VERSION_TEXT_IPV6 "yes"
#else
#define ZNC_VERSION_TEXT_IPV6 "no"
#endif

#ifdef HAVE_LIBSSL
#define ZNC_VERSION_TEXT_SSL "yes"
#else
#define ZNC_VERSION_TEXT_SSL "no"
#endif

#ifdef HAVE_THREADED_DNS
#define ZNC_VERSION_TEXT_DNS "threads"
#else
#define ZNC_VERSION_TEXT_DNS "blocking"
#endif

#ifdef HAVE_ICU
#define ZNC_VERSION_TEXT_ICU "yes"
#else
#define ZNC_VERSION_TEXT_ICU "no"
#endif

#ifdef HAVE_I18N
#define ZNC_VERSION_TEXT_I18N "yes"
#else
#define ZNC_VERSION_TEXT_I18N "no"
#endif

#define ZNC_COMPILE_OPTIONS_STRING                                    \
    "IPv6: " ZNC_VERSION_TEXT_IPV6 ", SSL: " ZNC_VERSION_TEXT_SSL     \
    ", DNS: " ZNC_VERSION_TEXT_DNS ", charset: " ZNC_VERSION_TEXT_ICU \
    ", i18n: " ZNC_VERSION_TEXT_I18N

#endif  // !ZNC_VERSION_H

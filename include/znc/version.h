#ifndef ZNC_VERSION_H
#define ZNC_VERSION_H

// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR  1
#define VERSION_MINOR  7
#define VERSION_PATCH  -1
// This one is for display purpose
#define VERSION_STR    "1.7.x"
// This one is for ZNCModInfo
#define VERSION        (VERSION_MAJOR + VERSION_MINOR / 10.0)

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
#ifndef VERSION_EXTRA
# define VERSION_EXTRA ""
#endif
extern const char* ZNC_VERSION_EXTRA;

#endif // !ZNC_VERSION_H

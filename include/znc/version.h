#ifndef _VERSION_H
#define _VERSION_H

// The following defines are for #if comparison (preprocessor only likes ints)
#define VERSION_MAJOR  1
#define VERSION_MINOR  3
// This one is for display purpose
#define VERSION        (VERSION_MAJOR + VERSION_MINOR / 10.0)

// You can add -DVERSION_EXTRA="stuff" to your CXXFLAGS!
extern const char* ZNC_VERSION_EXTRA;

#endif

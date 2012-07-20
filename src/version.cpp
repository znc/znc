#include <znc/version.h>

#ifdef VERSION_EXTRA
const char* ZNC_VERSION_EXTRA = VERSION_EXTRA;
#else
const char* ZNC_VERSION_EXTRA = "";
#endif


#  GZIP_FOUND:      whether gzip was found
#  GZIP_EXECUTABLE: path to the gzip

find_program(GZIP_EXECUTABLE NAMES gzip PATHS /bin /usr/bin /usr/local/bin)
if(GZIP_EXECUTABLE)
    set(GZIP_FOUND 1)
else()
    set(GZIP_FOUND 0)
endif()
if(NOT GZIP_FOUND AND GZIP_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find GZIP")
endif()
mark_as_advanced(GZIP_EXECUTABLE)

# - Try to find the sasl2 directory library
# Once done this will define
#
#  SASL2_FOUND - system has SASL2
#  SASL2_INCLUDE_DIR - the SASL2 include directory
#  SASL2_LIBRARIES - The libraries needed to use SASL2

# Copyright (c) 2006, 2007 Laurent Montel, <montel@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


if (SASL2_INCLUDE_DIR)
  # Already in cache, be silent
  set(SASL2_FIND_QUIETLY TRUE)
endif (SASL2_INCLUDE_DIR)

FIND_PATH(SASL2_INCLUDE_DIR sasl/sasl.h
)

# libsasl2 add for windows, because the windows package of cyrus-sasl2
# contains a libsasl2 also for msvc which is not standard conform
FIND_LIBRARY(SASL2_LIBRARIES NAMES sasl2 libsasl2
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Sasl2 DEFAULT_MSG SASL2_INCLUDE_DIR SASL2_LIBRARIES)

MARK_AS_ADVANCED(SASL2_INCLUDE_DIR SASL2_LIBRARIES)

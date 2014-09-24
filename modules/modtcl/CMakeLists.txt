add_library(modtcl MODULE modtcl.cpp)
include_directories(${TCL_INCLUDE_PATH})
target_link_libraries(modtcl ${ZNC_LIBRARIES} ${TCL_LIBRARY})
set_target_properties(modtcl PROPERTIES PREFIX "" SUFFIX .so LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/modules)

if(NOT DEVELOPER_BUILD)
    install(TARGETS modtcl LIBRARY DESTINATION ${INSTALL_MODDIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/modtcl.tcl
                  ${CMAKE_CURRENT_SOURCE_DIR}/binds.tcl DESTINATION ${INSTALL_DATADIR}/modules/modtcl)
endif()

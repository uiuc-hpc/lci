find_package(Doxygen)
option(BUILD_DOCUMENTATION "Create the Doxygen documentation" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
  if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif()

  set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/doxygen.in)
  set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

  set(publish2gh-pages_in ${CMAKE_CURRENT_SOURCE_DIR}/publish2gh-pages.sh.in)
  set(publish2gh-pages ${CMAKE_CURRENT_BINARY_DIR}/publish2gh-pages.sh)

  configure_file(${doxyfile_in} ${doxyfile} @ONLY)
  configure_file(${publish2gh-pages_in} ${publish2gh-pages} @ONLY)

  add_custom_target(
    doc
    COMMAND Doxygen::doxygen ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

  # install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc)
endif()

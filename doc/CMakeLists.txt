find_package(Doxygen)
option(BUILD_DOCUMENTATION "Create the Doxygen documentation" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
  if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif()

  # fetch doxygen awesome css
  include(FetchContent)
  FetchContent_Declare(
    doxygen-awesome-css
    URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/heads/main.zip
  )
  FetchContent_MakeAvailable(doxygen-awesome-css)

  # Save the location the files were cloned into This allows us to get the path
  # to doxygen-awesome.css
  FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

  set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  set(publish2gh-pages_in ${CMAKE_CURRENT_SOURCE_DIR}/publish2gh-pages.sh.in)
  set(publish2gh-pages ${CMAKE_CURRENT_BINARY_DIR}/publish2gh-pages.sh)

  configure_file(${doxyfile_in} ${doxyfile} @ONLY)
  configure_file(${publish2gh-pages_in} ${publish2gh-pages} @ONLY)
  # copy README.md into mainpage.md.in
  
  # Path to your input and output files
  set(MAINPAGE_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/mainpage.md.in")
  set(MAINPAGE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mainpage.md")
  set(README_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")

  # Read README.md content
  file(READ "${README_FILE}" README_CONTENTS)
  configure_file(${MAINPAGE_TEMPLATE} ${MAINPAGE_OUTPUT} @ONLY)

  add_custom_target(
    doc
    COMMAND Doxygen::doxygen ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

  # install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc)
endif()

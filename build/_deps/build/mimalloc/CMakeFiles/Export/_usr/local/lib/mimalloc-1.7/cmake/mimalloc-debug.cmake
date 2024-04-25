#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mimalloc" for configuration "Debug"
set_property(TARGET mimalloc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(mimalloc PROPERTIES
  IMPORTED_LOCATION_DEBUG "/usr/local/lib/mimalloc-1.7/libmimalloc-debug.so.1.7"
  IMPORTED_SONAME_DEBUG "libmimalloc-debug.so.1.7"
  )

list(APPEND _IMPORT_CHECK_TARGETS mimalloc )
list(APPEND _IMPORT_CHECK_FILES_FOR_mimalloc "/usr/local/lib/mimalloc-1.7/libmimalloc-debug.so.1.7" )

# Import target "mimalloc-static" for configuration "Debug"
set_property(TARGET mimalloc-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(mimalloc-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "/usr/local/lib/mimalloc-1.7/libmimalloc-debug.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS mimalloc-static )
list(APPEND _IMPORT_CHECK_FILES_FOR_mimalloc-static "/usr/local/lib/mimalloc-1.7/libmimalloc-debug.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)

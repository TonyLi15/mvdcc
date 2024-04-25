file(REMOVE_RECURSE
  "lib/libtpccrunner_static.a"
  "lib/libtpccrunner_static.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/tpccrunner_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()

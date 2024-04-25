file(REMOVE_RECURSE
  "libmasstree.a"
  "libmasstree.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C CXX)
  include(CMakeFiles/masstree.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()

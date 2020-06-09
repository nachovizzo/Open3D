# build
file(GLOB_RECURSE ALL_SOURCE_FILES "*.cpp")

# create object library
add_library(Keypoints OBJECT ${ALL_SOURCE_FILES})
ShowAndAbortOnWarning(Keypoints)

# Enforce 3rd party dependencies
add_dependencies(Keypoints build_all_3rd_party_libs)

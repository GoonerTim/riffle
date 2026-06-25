# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-src")
  file(MAKE_DIRECTORY "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-src")
endif()
file(MAKE_DIRECTORY
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-build"
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix"
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/tmp"
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp"
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/src"
  "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Program Files (x86)/projects/riffle/build-notest/_deps/simdjson-subbuild/simdjson-populate-prefix/src/simdjson-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

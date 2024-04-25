# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.16.3)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(mimalloc-populate NONE)

include(ExternalProject)
ExternalProject_Add(mimalloc-populate
                     "GIT_REPOSITORY" "https://github.com/microsoft/mimalloc" "GIT_TAG" "v1.7.1"
                    SOURCE_DIR          "/home/tonyli_15/serval/build/_deps/src/mimalloc"
                    BINARY_DIR          "/home/tonyli_15/serval/build/_deps/build/mimalloc"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)

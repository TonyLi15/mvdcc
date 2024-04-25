# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.16.3)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(masstree-populate NONE)

include(ExternalProject)
ExternalProject_Add(masstree-populate
                     "GIT_REPOSITORY" "https://github.com/wattlebirdaz/masstree-beta.git" "GIT_TAG" "MVTO"
                    SOURCE_DIR          "/home/tonyli_15/serval/build/_deps/src/masstree"
                    BINARY_DIR          "/home/tonyli_15/serval/build/_deps/build/masstree"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)

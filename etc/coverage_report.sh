#!/bin/bash
#
# Create a code-coverage report locally and upload one to codecov
# Should be run from the root directory

make distclean
CXXFLAGS="-fprofile-arcs -ftest-coverage" CFLAGS="-fprofile-arcs -ftest-coverage" ./configure
lcov --capture --directory . --output-file main_coverage.info
genhtml main_coverage.info --output-directory out

# Upload the coverage report
bash <(curl -s https://codecov.io/bash)

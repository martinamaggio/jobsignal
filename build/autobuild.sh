#!/bin/sh

################################################################################
# Avoid remembering all the automake toolchain, make will call this script
################################################################################

# create new build
mkdir bin

# copy source files
cd bin
cp -r ../src .
cp ../src/*.h .
cp ../build/configure.ac .
cp ../build/Makefile.am .

# using autotools
autoreconf --force --install
./configure
make

# made
cd ..

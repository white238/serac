#!/bin/bash

CMAKE_ARGS=-DCMAKE_CXX_COMPILER=clang++
CMAKE_ARGS=$CMAKE_ARGS -DENABLE_CLANGFORMAT=ON
CMAKE_ARGS=$CMAKE_ARGS -DCLANGFORMAT_EXECUTABLE=/usr/bin/clang-format

# Avoid error "fatal: detected dubious ownership in repository at '/github/workspace'"
REPO_PATH=/github/workspace
git config --global --add safe.directory "$REPO_PATH"
find "$REPO_PATH" -type d | while read -r dir; do
  git config --global --add safe.directory "$dir"
done

git submodule update --init --recursive

mkdir build && cd build 
cmake $CMAKE_ARGS ..
make style

#!/bin/bash

CMAKE_ARGS=-DCMAKE_CXX_COMPILER=clang++
CMAKE_ARGS="$CMAKE_ARGS -DENABLE_CLANGFORMAT=ON"
CMAKE_ARGS="$CMAKE_ARGS -DCLANGFORMAT_EXECUTABLE=/usr/bin/clang-format"
CMAKE_ARGS="$CMAKE_ARGS -DENABLE_MPI=ON"
CMAKE_ARGS="$CMAKE_ARGS -DMPI_C_COMPILER=/usr/bin/mpicc"
CMAKE_ARGS="$CMAKE_ARGS -DMPI_CXX_COMPILER=/usr/bin/mpic++"
CMAKE_ARGS="$CMAKE_ARGS -DMPI_Fortran_COMPILER=/usr/bin/mpif90"

# Avoid error "fatal: detected dubious ownership in repository at '/github/workspace'"
REPO_PATH=/github/workspace
git config --global --add safe.directory "$REPO_PATH"
find "$REPO_PATH" -type d | while read -r dir; do
  git config --global --add safe.directory "$dir"
done

# Set the branch to the PR's branch not a detached head
# Get the current commit SHA
current_commit_sha=$(git rev-parse HEAD)
# List all branches containing the current commit SHA
branches=$(git branch -r --contains $current_commit_sha)
# Remove '  origin/'
branch_name=$(echo "$branches" | sed 's/  origin\///')

echo "~~~~~~Found Branch~~~~~~~~"
echo $branch_name
echo "~~~~~~Branch~~~~~~~~"
git checkout $branch_name
git branch
echo "~~~~~~~~~~~~~~~~~~~~"

exit 1

git submodule update --init --recursive

mkdir build && cd build 
cmake $CMAKE_ARGS ..
make style

#!/bin/bash

CMAKE_ARGS=-DCMAKE_CXX_COMPILER=clang++
CMAKE_ARGS="$CMAKE_ARGS -DENABLE_CLANGFORMAT=ON"
CMAKE_ARGS="$CMAKE_ARGS -DCLANGFORMAT_EXECUTABLE=/usr/bin/clang-format"
CMAKE_ARGS="$CMAKE_ARGS -DENABLE_MPI=ON -DENABLE_FIND_MPI=ON"
CMAKE_ARGS="$CMAKE_ARGS -DMPI_C_COMPILER=/usr/bin/mpicc"
CMAKE_ARGS="$CMAKE_ARGS -DMPI_CXX_COMPILER=/usr/bin/mpic++"

# Avoid error "fatal: detected dubious ownership in repository at '/github/workspace'"
REPO_PATH=/github/workspace
git config --global --add safe.directory "$REPO_PATH"
find "$REPO_PATH" -type d | while read -r dir; do
  git config --global --add safe.directory "$dir"
done

git fetch

# Set the branch to the PR's branch not a detached head
# Get the current commit SHA
current_commit_sha=$(git rev-parse HEAD)
# List all branches containing the current commit SHA
branches=$(git branch -r --contains $current_commit_sha)

# Loop over the string split by whitespace
for branch in $branches; do
  # Skip items that start with "pull/"
  if [[ $branch == pull/* ]]; then
    continue
  fi
  if [[ $branch == origin/* ]]; then
  	branch=$(echo "$branch" | sed 's/origin\///')
  fi
  echo $branch
  git checkout $branch
  break
done

echo "~~~~~~Branch~~~~~~~~"
git branch
echo "~~~~~~~~~~~~~~~~~~~~"

git submodule update --init --recursive

mkdir build && cd build 
cmake $CMAKE_ARGS ..
make style

#!/bin/bash

# Script process:
# 1. Check for CMake in dependencies folder or on the machine: Install CMake in dependencies folder if needed
# 2. Build dependencies CMake project - New way of installing dependencies
# 3. Install remaining dependencies using the scripts
# 4. The aim is that over time, all dependencies will be delt with through CMake, and old scripts will be removed.

buildType=Release

while [[ $# -gt 0 ]]; do
  case $1 in
    -d|--debug)
      buildType=Debug
      shift # past argument
      ;;
    -r|--release)
      buildType=Release
      shift # past argument
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
  esac
done

# #############################################################################
# Check for CMake Version
# Install CMake in dependencies dir if needed
# #############################################################################

# CMake minimum version required (major.minor)
version_major=3
version_minor=16
version_patch=9

work_dir=$(pwd)
dependencies_dir=$work_dir/dependencies
dependencies_dir_archives=$dependencies_dir/arch
if [ ! -d $dependencies_dir ]; then
  mkdir $dependencies_dir
fi

if [ ! -d $dependecies_dir_arch ]; then
  mkdir $dependencies_dir_archives
fi

export PATH=$dependencies_dir/cmake/bin:$PATH

if [ $(command -v cmake) ]; then
  cmake_version_found=$(cmake --version)
  version_regex="([0-9]+)\.([0-9]+)\.([0-9]+)"
  if [[ $cmake_version_found =~ $version_regex ]]; then
    major_found=${BASH_REMATCH[1]}
    minor_found=${BASH_REMATCH[2]}
    echo "CMake found v$major_found.$minor_found."
    # Compare version found with version required
    if [[ $major_found > $version_major || ($major_found == $version_major  && $minor_found -ge $version_minor) ]]; then
      echo "CMake version ok"
    else
      echo "CMake version lower than required [v$version_major.$version_minor]"
      cmakeNeeded=true
    fi
  fi
else
  echo "CMake not found on the machine."
  cmakeNeeded=true
fi

if [ "$cmakeNeeded" = true ]; then
  echo "Installing CMake version $version_major.$version_minor in $dependencies_dir"
 
  cmake_folder="cmake-$version_major.$version_minor.$version_patch"

  arch=$(uname -m)
  if [ "$arch" = "x86_64" ]; then
    cmake_archive="cmake-$version_major.$version_minor.$version_patch-Linux-x86_64"
    build_needed=false
  else
    # Get archive to compile
    cmake_archive="cmake-$version_major.$version_minor.$version_patch"
    build_needed=true
  fi

  cd $dependencies_dir_archives

  if [ ! -d $cmake_folder ]; then
    if [ ! -f $cmake_archive ]; then
      wget http://www.cmake.org/files/v$version_major.$version_minor/$cmake_archive.tar.gz
    fi
    # Extract archive to specific folder name (archive name may differ depending on arch).
    tar -xzf $cmake_archive.tar.gz --transform s/$cmake_archive/$cmake_folder/
  fi

  if [ "$buildNeeded" = true ]; then
    cd $cmake_folder

    ./configure --prefix=$dependencies_dir/cmake
    make install

    cd ..
  else
    mv $cmake_folder $dependencies_dir/cmake
  fi

  rm -rf $cmake_archive
  cd $work_dir
fi

# #############################################################################
# Install remaining dependencies - original script method
# Deprecated method
# #############################################################################

cd $work_dir
perl sdk/scripts/linux-install_dependencies.pl --manifest-dir sdk/scripts/ --dependencies-dir $dependencies_dir
perl sdk/scripts/linux-install_dependencies.pl --manifest-dir designer/scripts/ --dependencies-dir $dependencies_dir
perl sdk/scripts/linux-install_dependencies.pl --manifest-dir extras/scripts/ --dependencies-dir $dependencies_dir

#!/bin/bash

set -e

if [ -z "$uise_compiler" ];
then
export uise_compiler=$1
fi

if [ -z "$uise_build" ];
then
export uise_build=$2
fi

if [ -z "$uise_compiler" ];
then
export uise_compiler=clang
fi

if [ -z "$uise_build" ];
then
export uise_build=release
fi

if [ -z "$build_workers" ];
then
export build_workers=4
fi

export CC=clang
export CXX=clang++
if [[ "$uise_compiler" == "gcc" ]];
then
    export CC=gcc
    export CXX=g++
fi

self_path="`dirname \"$0\"`"
export self_path="`( cd \"$self_path\" && pwd )`"

if [ -z "$deps_universal_root" ];
then
export deps_universal_root=$self_path/../../../../dracosha/deps
fi
export BOOST_ROOT=$deps_universal_root/root-$uise_compiler

if [ -z "$QT_HOME" ];
then
export QT_HOME=~/Qt6.0.0/6.0.0/gcc_64
fi

echo "deps=$deps_universal_root"
echo "boost_root=$BOOST_ROOT"
echo "qt_home=$QT_HOME"

if [[ "$uise_build" == "release" ]];
then
    export build_type=Release
fi
if [[ "$uise_build" == "debug" ]];
then
    export build_type=Debug
fi
if [[ "$uise_build" == "minsize_release" ]];
then
    export build_type=MinSizeRel
fi

export build_dir=$PWD/builds/build-$uise_compiler-$uise_build
export src_dir=$self_path/..

export PATH=$build_dir:$QT_HOME/bin:$boost_root/lib:$PATH

if [ -d $build_dir ];
    then
        rm -rf $build_dir
fi
mkdir -p $build_dir

export current_dir=$PWD
cd $build_dir

cmake -G "Unix Makefiles" -DUISE_TEST_JUNIT=ON -DCMAKE_BUILD_TYPE=$build_type $src_dir
cmake --build . -j$build_workers

if [[ "$uise_run_tests" == "1" ]];
then
    ctest -VV
fi

cd $current_dir

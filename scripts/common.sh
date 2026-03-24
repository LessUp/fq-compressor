#!/usr/bin/env bash
# scripts/common.sh - shared helpers for build/test/lint scripts

resolve_cmake_build_dir() {
    local project_dir=$1
    local preset=$2
    local marker=$3
    local build_dir="$project_dir/build/$preset"

    if [ -f "$build_dir/$marker" ]; then
        echo "$build_dir"
    elif [ -f "$build_dir/build/Debug/$marker" ]; then
        echo "$build_dir/build/Debug"
    elif [ -f "$build_dir/build/Release/$marker" ]; then
        echo "$build_dir/build/Release"
    elif [ -f "$build_dir/build/RelWithDebInfo/$marker" ]; then
        echo "$build_dir/build/RelWithDebInfo"
    else
        echo "$build_dir"
    fi
}

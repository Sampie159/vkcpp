#!/bin/bash

DIR=build
MODE=Debug
RUN=false

OPTSTR=":rRcs"

SHADERS=(tri.slang)

while getopts "${OPTSTR}" opt; do
    case ${opt} in
        r)
            RUN=true
            ;;
        R)
            DIR=release
            MODE=Release
            ;;
        c)
            rm -rf build release
            ;;
        s)
            echo "Compiling shaders!"
            for shader in "${SHADERS[@]}"; do
                echo "Compiling $shader"
                slangc shaders/tri.slang -target spirv -profile spirv_1_6 -emit-spirv-directly -O2 \
                       -fvk-use-entrypoint-name -o shaders/tri.spv

                if [[ $? = 0 ]]; then
                    echo "$shader compiled successfully"
                else
                    echo "$shader compilation failed!"
                    exit 1
                fi
            done
            echo "Shaders compiled succesfully"
            ;;
        ?) 
            echo "Invalid Option!"
            exit 1
            ;;
    esac
done

if [[ ! -d $DIR ]]; then
    cmake -B $DIR -S . -DCMAKE_BUILD_TYPE=$MODE -G Ninja
fi

cmake --build $DIR --parallel

if [[ ! $? = 0 ]]; then
    echo "Program compilation failed."
    exit 1
fi

if $RUN; then bin/playground; fi

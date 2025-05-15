#!/bin/bash
SCRIPT_DIRECTORY=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
OUT_DIRECTORY="out"

clear

g++ -march=native -g -I include src/llm.cpp -o "$OUT_DIRECTORY/llm_chat" $( pkg-config --libs llama) && \
gcc -march=native -g -I include src/server.c -o "$OUT_DIRECTORY/server" -lcivetweb -lpthread && \
clang-format --style="file:.clang-format" \
    -i \
    src/*.c src/*.cpp www/*.js

#!/bin/bash

clang++ -std=c++11 -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz

for swf in ~/projects/www/creaturebreeder.com/app/assets/flash/accessories/*.swf; do
    filename=$(basename "$swf")
    extension="${filename##*.}"
    filename="${filename%.*}"
    cmd="./a.out ${swf} ${filename} 200 200 ./out/${filename}.png"
    echo $cmd
    $cmd
done

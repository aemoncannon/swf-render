#!/bin/bash

clang++ -std=c++11 -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz

for swf in ~/projects/www/creaturebreeder.com/app/assets/flash/accessories/*.swf; do
    filename=$(basename "$swf")
    extension="${filename##*.}"
    filename="${filename%.*}"
    if [[ $filename == Small* ]] || [[ $filename == Young* ]];
    then
        cmd="./a.out -c ${filename} -w 200 -h 200 -p 60 -o ./out/${filename}.png ${swf}" 
    elif [[ $filename == Large* ]] || [[ $filename == Big* ]];
    then
        cmd="./a.out -c ${filename} -w 200 -h 200 -p 10 -o ./out/${filename}.png ${swf}" 
    else
        cmd="./a.out -c ${filename} -w 200 -h 200 -p 30 -o ./out/${filename}.png ${swf}" 
    fi
    echo "$cmd"
    $cmd
done

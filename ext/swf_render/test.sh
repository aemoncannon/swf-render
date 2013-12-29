set -e
clang++ -std=c++11 -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz
time ./a.out ~/projects/www/creaturebreeder.com/app/assets/flash/accessories/$1.swf $1 200 200 out.png
#time ./a.out Stipple.swf X

set -e
clang++ -std=c++11 -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz
#time ./a.out ~/projects/www/creaturebreeder.com/app/assets/flash/creatures/$1.swf $1 $2 $3 out.png
time ./a.out "$@"
#time ./a.out Stipple.swf X

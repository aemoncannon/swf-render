set -e
clang++ -std=c++11 -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz
time ./a.out AlphaTest.swf Incubator

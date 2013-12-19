clang++ -Wno-unused-value -I. agg_*.cpp tiny_*.cpp flash_rasterizer.cpp lodepng.cpp -lz
time ./a.out ../../Bamboo.swf

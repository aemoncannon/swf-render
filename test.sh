clang++ -Wno-unused-value -Ithird_party/agg/ -Isrc -Ithird_party/swfparser/ -Ithird_party/lodepng src/flash_rasterizer.cpp third_party/agg/*.cpp third_party/swfparser/*.cpp third_party/lodepng/*.cpp -lexpat -lz
./a.out Bamboo.swf

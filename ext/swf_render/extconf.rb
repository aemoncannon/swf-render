require 'mkmf'

#require 'fileutils'

#srcdir = File.dirname(__FILE__)
# $objs = (
#  Dir.glob("#{srcdir}/*.cpp") +
#  Dir.glob("#{srcdir}/src/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/agg/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/swfparser/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/lodepng/*.cpp")).collect{|cpp| cpp.gsub(".cpp", ".o")}

$CPPFLAGS += "-std=c++11 -Wno-unused-value "

create_makefile "swf_render"

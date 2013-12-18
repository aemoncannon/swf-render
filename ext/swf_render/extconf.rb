require 'mkmf'
require 'fileutils'

extension_name = "swf_render"

#srcdir = File.dirname(__FILE__)
# $objs = (
#  Dir.glob("#{srcdir}/*.cpp") +
#  Dir.glob("#{srcdir}/src/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/agg/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/swfparser/*.cpp") +
#  Dir.glob("#{srcdir}/third_party/lodepng/*.cpp")).collect{|cpp| cpp.gsub(".cpp", ".o")}

$CPPFLAGS += " -Wno-unused-value "

have_library("z")
have_library("stdc++")
create_makefile(extension_name)

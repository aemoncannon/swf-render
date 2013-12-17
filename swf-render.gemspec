Gem::Specification.new do |s|
  s.name    = "swf-render"
  s.version = "0.0.1"
  s.summary = "Render SWFs"
  s.author  = "Aemon Cannon"
  s.files = Dir.glob("ext/swf_render/*.{cpp}") +
            Dir.glob("ext/swf_render/src/*.{cpp}") +
            Dir.glob("ext/swf_render/third_party/swfparser/*.{cpp,h}") +
            Dir.glob("ext/swf_render/third_party/agg/*.{cpp,h}") +
            Dir.glob("ext/swf_render/third_party/lodepng/*.{cpp,h}")
  s.extensions << "ext/swf_render/extconf.rb"
  s.add_development_dependency "rake-compiler"
end
Gem::Specification.new do |s|
  s.name    = "swf_render"
  s.version = "0.0.2"
  s.summary = "Render SWFs"
  s.author  = "Aemon Cannon"
  s.files = Dir["ext/swf_render/*.{h,cpp}"] + Dir["lib/**/*"] + Dir['ext/**/extconf.rb']
  puts s.files
  s.platform = Gem::Platform::RUBY
  s.require_paths = [ 'lib', 'ext' ]
  s.extensions = Dir['ext/**/extconf.rb']
  puts s.extensions
end
require 'rake/extensiontask'
spec = Gem::Specification.load('swf_render.gemspec')

Rake::ExtensionTask.new do |ext|
  ext.name = 'swf_render'                # indicate the name of the extension.
  ext.ext_dir = 'ext/swf_render'
  ext.gem_spec = spec                     # indicate which gem specification will be used.
end

# add your default gem packing task
Gem::PackageTask.new(spec) do |pkg|
end

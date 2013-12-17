require 'rake/extensiontask'
spec = Gem::Specification.load('swf-render.gemspec')

Rake::ExtensionTask.new do |ext|
  ext.name = 'swf_render'                # indicate the name of the extension.

  ext.ext_dir = 'ext/swf_render'         # search for 'hello_world' inside it.
  ext.source_pattern = "*.{h,c,cpp}"        # monitor file changes to allow simple rebuild.
#  ext.config_options << '--with-foo'      # supply additional options to configure script.
  ext.gem_spec = spec                     # optionally indicate which gem specification
                                          # will be used.
end

# add your default gem packing task
Gem::PackageTask.new(spec) do |pkg|
end

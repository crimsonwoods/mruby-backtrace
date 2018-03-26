MRuby::Gem::Specification.new('mruby-backtrace') do |spec|
  spec.license = 'MIT'
  spec.authors = 'crimsonwoods'

  spec.search_package('libunwind-generic') if spec.respond_to?(:search_package)
end

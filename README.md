mruby-backtrace
====

Provide backtrace methods into __mruby__.

# How to build
----

Edit your 'build_config.rb'.

## mrbgem entry

    conf.gem "/path/to/your/mruby-backtrace"

## include paths (optional)

    conf.cc do |cc|
      cc.include_paths << "/path/to/your/libunwind/libraries"
    end

## library settings

    conf.linker do |linker|
      linker.libraries << %w(unwind unwind-#{target})
      linker.library_paths << "/path/to/your/libunwind/includes"
    end

## run make

    $ make


# How to use
----

## class Backtrace

    # put backtrace inside of RiteVM (display native C stack frames).
    Backtrace.put_vm

    # put backtrace inside of Ruby call stack.
    Backtrace.put_rb


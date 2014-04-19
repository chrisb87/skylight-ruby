require 'mkmf'
require 'yaml'
require 'logger'

# Must require 'rubygems/platform' vs. just requiring 'rubygems' to avoid a
# stack overflow bug on ruby 1.9.2.
require 'rubygems/platform'

class MultiIO
  def initialize(*targets)
     @targets = targets
  end

  def write(*args)
    @targets.each {|t| t.write(*args)}
  end

  def close
    @targets.each(&:close)
  end
end

log_file = File.open(File.expand_path("../install.log", __FILE__), "a")
LOG = Logger.new(MultiIO.new(STDOUT, log_file))

SKYLIGHT_REQUIRED = ENV.key?("SKYLIGHT_REQUIRED") && ENV['SKYLIGHT_REQUIRED'] !~ /^false$/i
PLATFORM = Gem::Platform.local

require_relative '../lib/skylight/version'
require_relative '../lib/skylight/util/native_ext_fetcher'

include Skylight::Util

# Handles terminating in the case of a failure. If we have a bug, we do not
# want to break our customer's deploy, but extconf.rb requires a Makefile to be
# present upon a successful exit. To satisfy this requirement, we create a
# dummy Makefile.
def fail(msg, type=:error)
  LOG.send type, msg

  if SKYLIGHT_REQUIRED
    exit 1
  else
    File.open("Makefile", "w") do |file|
      file.puts "default:"
      file.puts "install:"
    end

    exit
  end
end

def full_ruby_version
  "#{RUBY_VERSION}-#{RUBY_PATCHLEVEL}"
end

def is_darwin?
  PLATFORM.os == 'darwin'
end

def is_linux?
  PLATFORM.os == 'linux'
end

def is_cpu_profiling_supported?
  return false unless is_linux?
  v = RUBY_VERSION.split('.').map(&:to_i)
  return false unless v[0] && v[1]

  v[0] > 2 || (v[0] == 2 && v[1] >= 1)
end

libskylight_a   = File.expand_path('../libskylight.a', __FILE__)
libskylight_yml = File.expand_path('../libskylight.yml', __FILE__)
mri_header_dir  = ENV['MRI_HEADER_DIR'] || File.expand_path('../ruby-headers', __FILE__)
vm_core_h       = "#{mri_header_dir}/vm_core.h"

unless File.exist?(libskylight_a)
  # Ensure that libskylight.yml is present and load it
  unless File.exist?(libskylight_yml)
    fail "`#{libskylight_yml}` does not exist"
  end

  unless libskylight_info = YAML.load_file(libskylight_yml)
    fail "`#{libskylight_yml}` does not contain data"
  end

  unless version = libskylight_info["version"]
    fail "libskylight version missing from `#{libskylight_yml}`"
  end

  unless checksums = libskylight_info["checksums"]
    fail "libskylight checksums missing from `#{libskylight_yml}`"
  end

  arch = "#{PLATFORM.os}-#{PLATFORM.cpu}"

  unless checksum = checksums[arch]
    fail "no checksum entry for requested architecture -- " \
             "this probably means the requested architecture is not supported; " \
             "arch=#{arch}; available=#{checksums.keys}", :info
  end

  begin
    opts = {
      version:  version,
      target:   libskylight_a,
      checksum: checksum,
      arch:     arch,
      required: SKYLIGHT_REQUIRED,
      logger:   LOG
    }

    if is_cpu_profiling_supported?
      unless File.exist?(vm_core_h)
        opts[:ruby_version] = full_ruby_version
        opts[:header_dir] = mri_header_dir
      end
    end

    res = NativeExtFetcher.fetch(opts)

    unless res
      fail "could not fetch archive -- aborting skylight native extension build"
    end
  rescue => e
    fail "unable to fetch native extension; msg=#{e.message}\n#{e.backtrace.join("\n")}"
  end
end

#
#
# ===== By this point, libskylight.a is present =====
#
#

if is_darwin?
  # Match libskylight's min OS X version. The ruby extension's min version must
  # match libskylight's in order to get things to work.
  $CFLAGS << " -mmacosx-version-min=10.7"
end

# TODO: Only compile CPU profiling support on ruby 2.1
if File.exist?(vm_core_h) && find_header("vm_core.h", mri_header_dir)
  $defs << "-DHAVE_CPU_PROFILING"
else
  LOG.info("private MRI headers not present; disabling CPU profiling");
end

unless have_header('dlfcn.h')
  abort "dlfcn.h missing"
end

unless find_library("skylight", "sk_high_res_time", ".")
  abort "invalid libskylight"
end

# Treat all warnings as errors
$CFLAGS << " -Werror"

if is_darwin?
  # Link against pthread
  $LDFLAGS << " -lpthread"
else
  $LDFLAGS << " -Wl,--version-script=skylight.map"
  $LDFLAGS << " -lrt -ldl -lm -lpthread"
end

CONFIG['warnflags'].gsub!('-Wdeclaration-after-statement', '')

create_makefile 'skylight_native', '.'

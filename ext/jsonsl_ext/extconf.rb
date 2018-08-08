# vim: et ts=2 sts=2 sw=2

# Author:: Couchbase <info@couchbase.com>
# Copyright:: 2018 Couchbase, Inc.
# License:: Apache License, Version 2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

require 'rbconfig'
require 'mkmf'

def define(macro, value = nil)
  $defs.push("-D #{[macro.upcase, Shellwords.shellescape(value)].compact.join('=')}")
end

$CFLAGS << ' -pedantic -Wall -Wextra -Werror '
if ENV['DEBUG_BUILD']
  $CFLAGS.gsub!(/\W-Wp,-D_FORTIFY_SOURCE=\d+\W/, ' ')
  $CFLAGS << ' -ggdb3 -O0 '
end
if ENV['FETCH_JSONSL']
  require 'open-uri'
  ['jsonsl.c', 'jsonsl.h'].each do |filename|
    remote = open("https://github.com/mnunberg/jsonsl/raw/master/#{filename}")
    File.write(File.join(__dir__, filename), remote.read)
  end
  Dir.chdir(__dir__) do
    system('patch < jsonsl.h.patch')
  end
  sha1 = open('https://github.com/mnunberg/jsonsl').read[/commit-tease-sha.*?commit\/([a-f0-9]+)/m, 1]
  File.write(File.join(__dir__, 'jsonsl.rev'), sha1)
end

define('JSONSL_REVISION', File.read(File.join(__dir__, 'jsonsl.rev')).strip.inspect)

create_header('jsonsl_config.h')
create_makefile('jsonsl_ext')

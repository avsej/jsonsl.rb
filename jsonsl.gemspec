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

lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'jsonsl/version'

Gem::Specification.new do |spec|
  spec.name = 'jsonsl'
  spec.version = JSONSL::VERSION
  spec.author = 'Sergey Avseyev'
  spec.email = 'sergey.avseyev@gmail.com'

  spec.summary = 'JSONSL is a high-speed JSON lexer'
  spec.description = 'This is ruby bindings for jsonsl library'
  spec.homepage = 'https://github.com/avsej/jsonsl.rb'

  spec.files = `git ls-files -z`.split("\x0").reject do |f|
    f.match(%r{^test/})
  end
  spec.bindir = 'exe'
  spec.executables = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.extensions = spec.files.grep(%r{^ext/.*/extconf.rb})
  spec.require_paths = ['lib']

  spec.add_development_dependency 'bundler', '~> 1.16'
  spec.add_development_dependency 'rake', '~> 12.3'
  spec.add_development_dependency 'minitest', '~> 5.11'
end

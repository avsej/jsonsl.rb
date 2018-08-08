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

require 'bundler/gem_tasks'
require 'rake/testtask'


module Bundler
  class GemHelper

   def tag_version
      sh "git tag -s -m \"Version #{version}\" #{version_tag}"
      Bundler.ui.confirm "Tagged #{version_tag}."
      yield if block_given?
    rescue
      Bundler.ui.error "Untagging #{version_tag} due to error."
      sh_with_code "git tag -d #{version_tag}"
      raise
    end
  end
end

Rake::TestTask.new(:test) do |t|
  t.libs << 'test'
  t.libs << 'lib'
  t.test_files = FileList['test/**/*_test.rb']
end

require "rake/extensiontask"

def gemspec
  @clean_gemspec ||= eval(File.read(File.expand_path('jsonsl.gemspec', File.dirname(__FILE__))))
end

Rake::ExtensionTask.new("jsonsl_ext", gemspec) do |ext|
  ext.ext_dir = "ext/jsonsl_ext"
  CLEAN.include "#{ext.lib_dir}/*.#{RbConfig::CONFIG['DLEXT']}"
end
Rake::Task['test'].prerequisites.unshift('compile')

task :default => :test

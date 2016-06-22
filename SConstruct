#******************************************************************
#
# Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

##
# The main build script
#
##
import os

# Load common build config
SConscript('build_common/SConscript')

Import('env')

# if env.get('VERBOSE'):
#     print "ENV:"
#     print env.Dump()
#     print "START"

if os.environ.get('TERM') != None:
	env['ENV']['TERM'] = os.environ['TERM']

# Load extra options
SConscript('extra_options.scons')

target_os = env.get('TARGET_OS')
if target_os == 'arduino':
	SConscript('arduino.scons')

# By default, src_dir is current dir, the build_dir is:
#     ./out/<target_os>/<target_arch>/<release or debug>/
#
# The build_dir is a variant directory of the source directory(You can
# consider build_dir as a soft link to src_dir, for detail please refer to:
#     http://www.scons.org/doc/production/HTML/scons-user.html#f-VariantDir
#
# Any way, to make the output is in build_dir, when load scripts, the path should
# be relevant to build_dir.
build_dir = env.get('BUILD_DIR')

#GAR default: build the kernel
# Build 'resource' sub-project
SConscript(build_dir + 'resource/SConscript')

#GAR FIXME: only build services on demand
# # Build 'service' sub-project
# SConscript(build_dir + 'service/SConscript')

#GAR FIXME: only make examples on demand
# if target_os not in ['arduino','ios', 'android']:
# 	SConscript(build_dir + 'examples/OICMiddle/SConscript')

#GAR FIXME: only build cloud on demand
# # Build "cloud" sub-project
# SConscript(build_dir + 'cloud/SConscript')

#GAR FIXME: only on demand
# # Build "plugin interface" sub-project
# SConscript(build_dir + 'plugins/SConscript')

#GAR: only build the Java SDK on demand:
if env.get('BUILD_JAVA') == 'ON' or target_os == 'android':
	if env.get('JAVA_HOME') != None:
		SConscript(build_dir + 'java/SConscript')

#GAR FIXME: only build the Android SDK on demand

# Append targets information to the help information, to see help info, execute command line:
#     $ scon [options] -h
env.PrintTargets()

# Print bin upload command line (arduino only)
if target_os == 'arduino':
	env.UploadHelp()

#GAR FIXME
# # to install the generated pc file into custome prefix location
# env.UserInstallTargetPCFile('iotivity.pc', 'iotivity.pc')


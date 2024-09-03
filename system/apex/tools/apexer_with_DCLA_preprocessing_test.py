#!/usr/bin/env python3
#
# Copyright (C) 2020 The Android Open Source Project
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

"""Unit tests for apexer_with_DCLA_preprocessing."""
import hashlib
import importlib.resources
import os
import shutil
import stat
import subprocess
import tempfile
from typing import List, BinaryIO
import unittest
import zipfile

TEST_PRIVATE_KEY = os.path.join('testdata', 'com.android.example.apex.pem')
TEST_APEX = 'com.android.example.apex'

# In order to debug test failures, set DEBUG_TEST to True and run the test from
# local workstation bypassing atest, e.g.:
# $ m apexer_with_DCLA_preprocessing_test && \
#   out/host/linux-x86/nativetest64/apexer_with_DCLA_preprocessing_test/\
#   apexer_with_DCLA_preprocessing_test
#
# the test will print out the command used, and the temporary files used by the
# test.
DEBUG_TEST = False

def resources():
  return importlib.resources.files('apexer_with_DCLA_preprocessing_test')

# TODO: consolidate these common test utilities into a common python_library_host
# to be shared across tests under system/apex
def run_command(cmd: List[str]) -> None:
  """Run a command."""
  try:
    if DEBUG_TEST:
      cmd_str = ' '.join(cmd)
      print(f'\nRunning: \n{cmd_str}\n')
    subprocess.run(
        cmd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
  except subprocess.CalledProcessError as err:
    print(err.stderr)
    print(err.output)
    raise err

def get_digest(file_path: str) -> str:
  """Get sha512 digest of a file """
  digester = hashlib.sha512()
  with open(file_path, 'rb') as f:
    bytes_to_digest = f.read()
    digester.update(bytes_to_digest)
    return digester.hexdigest()

class ApexerWithDCLAPreprocessingTest(unittest.TestCase):

  def setUp(self):
    self._to_cleanup = []
    self.unzip_host_tools()

  def tearDown(self):
    if not DEBUG_TEST:
      for i in self._to_cleanup:
        if os.path.isdir(i):
          shutil.rmtree(i, ignore_errors=True)
        else:
          os.remove(i)
      del self._to_cleanup[:]
    else:
      print('Cleanup: ' + str(self._to_cleanup))

  def create_temp_dir(self) -> str:
    tmp_dir = tempfile.mkdtemp()
    self._to_cleanup.append(tmp_dir)
    return tmp_dir

  def expand_apex(self, apex_file: str | BinaryIO) -> None:
    """expand an apex file include apex_payload."""
    apex_dir = self.create_temp_dir()
    with zipfile.ZipFile(apex_file, 'r') as apex_zip:
      apex_zip.extractall(apex_dir)
    payload_img = os.path.join(apex_dir, 'apex_payload.img')
    extract_dir = os.path.join(apex_dir, 'payload_extract')
    os.mkdir(extract_dir)
    run_command([self.debugfs_static, payload_img, '-R', f'rdump / {extract_dir}'])

    # remove /etc and /lost+found and /payload_extract/apex_manifest.pb
    lost_and_found = os.path.join(extract_dir, 'lost+found')
    etc_dir = os.path.join(extract_dir, 'etc')
    os.remove(os.path.join(extract_dir, 'apex_manifest.pb'))
    if os.path.isdir(lost_and_found):
      shutil.rmtree(lost_and_found)
    if os.path.isdir(etc_dir):
      shutil.rmtree(etc_dir)

    return apex_dir

  def unzip_host_tools(self) -> None:
    host_tools_dir = self.create_temp_dir()
    with (
      resources().joinpath('apexer_test_host_tools.zip').open(mode='rb') as host_tools_zip_resource,
      resources().joinpath(TEST_PRIVATE_KEY).open(mode='rb') as key_file_resource,
      resources().joinpath('apexer_with_DCLA_preprocessing').open(mode='rb') as apexer_wrapper_resource,
    ):
      with zipfile.ZipFile(host_tools_zip_resource, 'r') as zip_obj:
        zip_obj.extractall(host_tools_dir)
      apexer_wrapper = os.path.join(host_tools_dir, 'apexer_with_DCLA_preprocessing')
      with open(apexer_wrapper, 'wb') as f:
        shutil.copyfileobj(apexer_wrapper_resource, f)
      key_file = os.path.join(host_tools_dir, 'key.pem')
      with open(key_file, 'wb') as f:
        shutil.copyfileobj(key_file_resource, f)


    self.apexer_tool_path = os.path.join(host_tools_dir, 'bin')
    self.apexer_wrapper = apexer_wrapper
    self.key_file = key_file
    self.debugfs_static = os.path.join(host_tools_dir, 'bin/debugfs_static')
    self.android_jar = os.path.join(host_tools_dir, 'bin/android.jar')
    self.apexer = os.path.join(host_tools_dir, 'bin/apexer')
    os.chmod(apexer_wrapper, stat.S_IRUSR | stat.S_IXUSR);
    for i in ['apexer', 'deapexer', 'avbtool', 'mke2fs', 'sefcontext_compile', 'e2fsdroid',
      'resize2fs', 'soong_zip', 'aapt2', 'merge_zips', 'zipalign', 'debugfs_static',
      'signapk.jar', 'android.jar']:
      file_path = os.path.join(host_tools_dir, 'bin', i)
      if os.path.exists(file_path):
        os.chmod(file_path, stat.S_IRUSR | stat.S_IXUSR);


  def test_DCLA_preprocessing(self):
    """test DCLA preprocessing done properly."""
    with resources().joinpath(TEST_APEX + '.apex').open(mode='rb') as apex_file:
      apex_dir = self.expand_apex(apex_file)

    # create apex canned_fs_config file, TEST_APEX does not come with one
    canned_fs_config_file = os.path.join(apex_dir, 'canned_fs_config')
    with open(canned_fs_config_file, 'w') as f:
      # add /lib/foo.so file
      lib_dir = os.path.join(apex_dir, 'payload_extract', 'lib')
      os.makedirs(lib_dir)
      foo_file = os.path.join(lib_dir, 'foo.so')
      with open(foo_file, 'w') as lib_file:
        lib_file.write('This is a placeholder lib file.')
      foo_digest = get_digest(foo_file)

      # add /lib dir and /lib/foo.so in canned_fs_config
      f.write('/lib 0 2000 0755\n')
      f.write('/lib/foo.so 1000 1000 0644\n')

      # add /lib/bar.so file
      lib_dir = os.path.join(apex_dir, 'payload_extract', 'lib64')
      os.makedirs(lib_dir)
      bar_file = os.path.join(lib_dir, 'bar.so')
      with open(bar_file, 'w') as lib_file:
        lib_file.write('This is another placeholder lib file.')
      bar_digest = get_digest(bar_file)

      # add /lib dir and /lib/foo.so in canned_fs_config
      f.write('/lib64 0 2000 0755\n')
      f.write('/lib64/bar.so 1000 1000 0644\n')

      f.write('/ 0 2000 0755\n')
      f.write('/apex_manifest.pb 1000 1000 0644\n')

    # call apexer_with_DCLA_preprocessing
    manifest_file = os.path.join(apex_dir, 'apex_manifest.pb')
    build_info_file = os.path.join(apex_dir, 'apex_build_info.pb')
    apex_out = os.path.join(apex_dir, 'DCLA_preprocessed_output.apex')
    run_command([self.apexer_wrapper,
                 '--apexer', self.apexer,
                 '--canned_fs_config', canned_fs_config_file,
                 os.path.join(apex_dir, 'payload_extract'),
                 apex_out,
                 '--',
                 '--android_jar_path', self.android_jar,
                 '--apexer_tool_path', self.apexer_tool_path,
                 '--key', self.key_file,
                 '--manifest', manifest_file,
                 '--build_info', build_info_file,
                 '--payload_fs_type', 'ext4',
                 '--payload_type', 'image',
                 '--force'
                 ])

    # check the existence of updated canned_fs_config
    updated_canned_fs_config = os.path.join(apex_dir, 'updated_canned_fs_config')
    self.assertTrue(
        os.path.isfile(updated_canned_fs_config),
        'missing updated canned_fs_config file named updated_canned_fs_config')

    # check the resulting apex, it should have /lib/foo.so/<hash>/foo.so and
    # /lib64/bar.so/<hash>/bar.so
    result_apex_dir = self.expand_apex(apex_out)
    replaced_foo = os.path.join(
        result_apex_dir, f'payload_extract/lib/foo.so/{foo_digest}/foo.so')
    replaced_bar = os.path.join(
        result_apex_dir, f'payload_extract/lib64/bar.so/{bar_digest}/bar.so')
    self.assertTrue(
        os.path.isfile(replaced_foo),
        f'expecting /lib/foo.so/{foo_digest}/foo.so')
    self.assertTrue(
        os.path.isfile(replaced_bar),
        f'expecting /lib64/bar.so/{bar_digest}/bar.so')

if __name__ == '__main__':
  unittest.main(verbosity=2)

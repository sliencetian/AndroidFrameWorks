#!/usr/bin/env python3
#
# Copyright (C) 2024 The Android Open Source Project
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
""" A tool to rewrite source files to turn individual tests that run in main()
into gtests"""

import argparse
import os
from pathlib import Path

def parse_args() -> argparse.Namespace:
  """Parse commandline arguments."""

  parser = argparse.ArgumentParser()
  parser.add_argument('--suite', help='specify test suite name')
  parser.add_argument('--test_name_prefix', help='specify test name prefix')
  parser.add_argument('--main_no_arguments', action='store_true',
                      help='standalone test main function is declared with no arguments')
  parser.add_argument('--predicate', default='NULL',
                      help='name of function that converts return value of main to boolean pass signal')
  parser.add_argument('--in', required=True, dest='in_path', type=Path,
                      help='specify the input file')
  parser.add_argument('--out', required=True, dest='out_path', type=Path,
                      help='specify the output file')
  return parser.parse_args()


def rewrite_test_src(in_path: Path, out_path: Path, suite_name: str,
                     test_name: str, main_no_arguments: bool, predicate: str):
  with open(out_path, 'w', encoding='utf8') as out_file:
    with open(in_path, encoding='utf8') as in_file:
      out_file.write('// Automatically inserted by gtestifier\n')
      out_file.write('#ifndef GTESTIFIER_TEST\n')
      if suite_name:
        out_file.write(f'#define GTESTIFIER_SUITE {suite_name}\n')
        out_file.write(f'#define GTESTIFIER_TEST {test_name}\n')
        out_file.write(f'#define GTESTIFIER_MAIN_NO_ARGUMENTS {int(main_no_arguments)}\n')
        out_file.write(f'#define GTESTIFIER_PREDICATE {predicate}\n')
        out_file.write('#include <gtestifier.h>\n')
        out_file.write('#endif\n')
        out_file.write('// End automatically inserted by gtestifier\n')
        out_file.write('\n')
        out_file.write(in_file.read())


def path_to_test_name(in_path: Path, test_name_prefix: str):
  name = ''.join([c for c in in_path.stem if c == '_' or str.isalnum(c)])
  return test_name_prefix + name


def main():
  """Program entry point."""
  args = parse_args()

  rewrite_test_src(args.in_path, args.out_path, args.suite,
                   path_to_test_name(args.in_path, args.test_name_prefix),
                   args.main_no_arguments, args.predicate)


if __name__ == '__main__':
  main()

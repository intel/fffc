#! /usr/bin/env python3

# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: MIT

"""
generate.py

This takes a debuggable executable and an output directory and turns it into a
directory full of compile unit derived source files.
"""

import argparse
import os
import shutil
import sys
import pathlib
import traceback

from fffc.dwarf_to_c import Executable


DESCRIPTION = "An easy-to-use fuzzer generator for programs written in C."


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument(
        "--headers_only", "-H", action="store_true", help="Only convert dwarf to c."
    )
    parser.add_argument(
        "--overwrite", "-O", action="store_true", help="Overwrite an existing output directory."
    )
    parser.add_argument(
        "targets", nargs="+", help="The program(s) to generate a fuzzer for."
    )
    parser.add_argument("output", help="The destination directory for fuzzers.")
    arguments = parser.parse_args()

    for target in arguments.targets:
        path = pathlib.Path(arguments.output) / target
        if path.exists():
            if not arguments.overwrite:
                msg = "Cannot continue without clobbering %s. " % str(path)
                msg += "Please select a different path, or use the --overwrite option."
                print(msg)
                return
            else:
                try:
                    shutil.rmtree(str(path))
                except Exception:
                    print("Unable to delete existing path %s." % str(path))
                    return

    for target in arguments.targets:
        path = pathlib.Path(arguments.output) / target
        try:
            # build the dependencies from the toplevel
            exe = Executable(target, target, path, arguments.headers_only, True)
            exe.generate_sources()
        except Exception as ex:
            traceback.print_exc()
            continue
        Executable.libraries = []


if __name__ == "__main__":
    main()

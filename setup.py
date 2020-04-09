#! /usr/bin/env python3

# Copyright (C) 2020 Intel Corporation
# SPDX-License-Identifier: MIT

from setuptools import setup

setup(
    name="fffc",
    version="0.1",
    description="The easiest to use fuzzer... IN THE WORLD",
    author="Geremy Condra",
    author_email="geremy.condra@intel.com",
    license="MIT",
    packages=["fffc"],
    scripts=["scripts/fffc_log_inspector", "scripts/fffc_dedup_crashes", "scripts/fffc_crashtool", "scripts/fffc_internal_crashtool"],
    entry_points={"console_scripts": ["fffc = fffc.generate:main"]},
    package_data={"fffc": ["templates/*"]},
    install_requires=["pyelftools", "pycparser"],
    zip_safe=False,
)

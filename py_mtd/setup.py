#!/usr/bin/env python
from distutils.core import setup, Extension

_mod = Extension('_py_mtd', sources=['py_mtd.c'])

setup(
    name='py_mtd',
    version='1.0',
    description='Utilities for working with Linux MTD',
    author='Denton Gentry',
    author_email='dgentry@google.com',
    url='',
    packages=['py_mtd'],
    ext_modules=[_mod],
    package_dir={'py_mtd': ''},
)

#!/usr/bin/env python

from __future__ import print_function

import os
from distutils import sysconfig

# get the shared library path
libdir = sysconfig.get_config_var('LIBDIR')
basedir = os.path.dirname(libdir)
print(basedir)

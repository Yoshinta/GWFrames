#! /usr/bin/env python

"""
Installation file for python code associated with the paper "Angular
velocity of gravitational radiation from precessing binaries and the
corotating frame".

To build this code and run it in place, run
    python setup.py build_ext --inplace
then open python and type 'import GWFrames' in
the current directory.

To install in the user's directory, run
    python setup.py install --user
Now, 'import GWFrames' may be run from a python
instance started in any directory on the system.
"""

## If PRD won't let me keep a subdirectory, make one
from os.path import exists
from os import makedirs
if not exists('GWFrames') :
    makedirs('GWFrames')
from shutil import copyfile
if not exists('GWFrames/plot.py') :
    copyfile('plot.py', 'GWFrames/plot.py')

## distutils doesn't build swig modules in the correct order by
## default -- the python module is installed first.  This will pop
## 'build_ext' to the beginning of the command list.
from distutils.command.build import build
build.sub_commands = sorted(build.sub_commands, key=lambda sub_command: int(sub_command[0]!='build_ext'))

## We also need to copy the SWIG-generated python script GWFrames.py
## to GWFrames/__init__.py so that it gets installed correctly.
from distutils.command.build_ext import build_ext as _build_ext
from distutils.file_util import copy_file
class build_ext(_build_ext):
    """Specialized Python source builder for moving SWIG module."""
    def run(self):
        _build_ext.run(self)
        copy_file('GWFrames.py', 'GWFrames/__init__.py')

## Now import the basics
from distutils.core import setup, Extension
from subprocess import check_output, CalledProcessError
from os import devnull, environ

## See if GSL_HOME is set; if so, use it
if "GSL_HOME" in environ :
    IncDirs = [environ["GSL_HOME"]+'/include', '/opt/local/include']
    LibDirs = [environ["GSL_HOME"]+'/lib', '/opt/local/lib']
else :
    IncDirs = ['/opt/local/include']
    LibDirs = ['/opt/local/lib']

## Remove a compiler flag that doesn't belong there for C++
import distutils.sysconfig as ds
cfs=ds.get_config_vars()
for key, value in cfs.iteritems() :
    if(type(cfs[key])==str) :
        cfs[key] = value.replace('-Wstrict-prototypes', '')

## Try to determine an automatic version number for this
try :
    with open(devnull, "w") as fnull :
        GitRev = check_output('git rev-parse HEAD', shell=True, stderr=fnull)[:-1]
        CodeRevision = '"{0}"'.format(GitRev)
        PackageVersion = GitRev[:9]
except (NameError, CalledProcessError) :
    CodeRevision = '"PaperVersion3"'
    PackageVersion = '3'

## Read in the license
try :
    with open('LICENSE', 'r') as myfile :
        License=myfile.read()
except IOError :
    License = 'See LICENSE file in the source code for details.'

## This does the actual work
setup(name="GWFrames",
      version=PackageVersion,
      description='Angular velocity of gravitational radiation from precessing binaries and the corotating frame',
      long_description="""
      This package implements various methods described in the paper
      "Angular velocity of gravitational radiation from precessing
      binaries and the corotating frame".  In particular, it
      implements three useful classes -- Quaternion, Waveform, and
      PNWaveform -- along with various methods for each.""",
      author='Michael Boyle',
      author_email='boyle@astro.cornell.edu',
      url='https://github.com/MOBle',
      license=License,
      packages = ['GWFrames'],
      # py_modules = ['GWFrames'],
      scripts = ['Scripts/RunExtrapolations.py', 'Scripts/ConvertGWDatToH5.py'],
      ext_modules = [
        Extension('_GWFrames', ['Quaternions.cpp',
                                'Utilities.cpp',
                                'Waveforms.cpp',
                                'PNWaveforms.cpp',
                                'GWFrames.i'],
                  depends = ['Quaternions.hpp',
                             'Utilities.hpp',
                             'Waveforms.hpp',
                             'PNWaveforms.hpp',
                             'PNWaveforms_TaylorT1Spin.ipp',
                             'PNWaveforms_PolarizationModes.ipp',
                             'Errors.hpp',
                             'GWFrames_Doc.i'],
                  include_dirs=IncDirs,
                  library_dirs=LibDirs,
                  libraries=['gsl', 'gslcblas'],
                  define_macros = [('CodeRevision', CodeRevision)],
                  language='c++',
                  swig_opts=['-globals', 'constants', '-c++'],
                  extra_link_args=['-fPIC'],
                  # extra_link_args=['-fPIC'], # Shouldn't be necessary...
                  # extra_compile_args=['-ffast-math'] # DON'T USE fast-math!!!  It makes it impossible to detect NANs
                  )
        ],
      # classifiers = ,
      # distclass = ,
      # script_name = ,
      # script_args = ,
      # options = ,
      # license = ,
      # keywords = ,
      # platforms = ,
      # cmdclass = ,
      cmdclass={'build_ext': build_ext},
      # data_files = ,
      # package_dir = 
      )

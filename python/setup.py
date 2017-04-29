import sys

from setuptools import setup, Extension

v = '2'
if (sys.version_info > (3, 0)):
    v = '3'

iptc = Extension('iptcdata', sources=['python%s/pyiptcdata.c' % (v),
                                      'python%s/pyiptcdatamod.c' % (v),
                                      'python%s/pyiptcdataset.c' % (v)],
                 libraries=['iptcdata'],
                 depends=['python%s/pyiptcdata.h' % (v)])

setup (name = 'iptcdata',
       version = '1.0.5',
       description = 'A library for manipulating the IPTC metadata',
       author = 'Ian Wienand',
       author_email = 'ian@wienand.org',
       url = 'https://github.com/ianw/libiptcdata',
       long_description = '''
libiptcdata is a library for manipulating the International Press
Telecommunications Council (IPTC) metadata stored within multimedia
files such as images.  The library provides routines for parsing,
viewing, modifying, and saving this metadata.  The library is licensed
under the GNU Library General Public License (GNU LGPL).

The library implements the standard described at http://www.iptc.org/IIM
       ''',
       ext_modules = [iptc])

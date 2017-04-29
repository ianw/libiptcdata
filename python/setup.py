from setuptools import setup, Extension

iptc = Extension('iptcdata', sources=['python3/pyiptcdata.c',
                                      'python3/pyiptcdatamod.c',
                                      'python3/pyiptcdataset.c'],
                 libraries=['iptcdata'],
                 depends=['python3/pyiptcdata.h'])

setup (name = 'iptcdata',
       version = '1.0.4',
       description = 'iptcdata',
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

The code itself was inspired by the libexif library:
http://sourceforge.net/projects/libexif, written by Lutz Müller.
Together, libexif and libiptcdata provide a complete metadata
solution for image files.
       ''',
       ext_modules = [iptc])

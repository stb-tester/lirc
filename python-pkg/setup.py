''' LIRC Python API, provisionary. Includes a C extension. Requires lirc,
including header files, installed
'''

import subprocess
import os.path

from setuptools import setup, Extension

if os.path.exists('../lirc.pc'):
    # development tree
    cflags = ['-I../lib']
    libs = ['-L../lib/.libs']
else:
    cflags = subprocess.check_output(["pkg-config", "--cflags", 'lirc'])
    cflags = cflags.decode("ascii").strip().split()
    libs =  subprocess.check_output(["pkg-config", "--libs", 'lirc'])
    libs = libs.decode("ascii").strip().split()

c_module = Extension('_client',
                     sources=['lirc/_client.c'],
                     libraries=['lirc_client'],
                     extra_compile_args=cflags,
                     extra_link_args=libs)
setup(
    name = 'lirc',
    version = "0.9.5",
    author = "Alec Leamas",
    author_email = "leamas@nowhere.net",
    url = "http://sf.net/p/lirc",
    description = "LIRC python API",
    keywords = "lirc asyncio API",
    long_description = open('README.rst', encoding='utf-8').read(),
    license = "GPLv2+",
    packages = ['lirc',],
    ext_modules = [c_module],
    include_package_data = True,
    test_suite = 'tests.test_client',
    entry_points = {'console_scripts': ['lirctool=lirc.lirctool:main']},
    classifiers = [
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: GNU General Public License v2 or later (GPLv2+)',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'Natural Language :: English',
        'Operating System :: Unix',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Topic :: System :: Hardware'
    ]
)

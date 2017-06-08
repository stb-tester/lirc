Python API for lirc
===================

This package is created from the upstream lirc project's main build
flow. If you have installed LIRC  either from a package or directly
from sources you don't need this. The primary usecase is when installing
the LIRC api in a virtual environment.

The python package is not independent of the systemwide LIRC installation,
it is just an API to access the system-wide LIRC from e. g, a virtual env.

The package is python3 only.

Dependencies
------------

    - A LIRC installation complete with header files (typically
      the -dev or -devel packages, or built from source).
    - Tests requires
          - ncat - see http://nmap.org/
          - expect - see  http://expect.nist.gov/

Installation
------------

    - Build and install lirc from source or install a LIRC package.
    - Locate the python source distribution, typically in a location like
      /usr/share/lirc/lirc-0.10.0.tar.gz
    - Use cmd like  *pip3 install /usr/share/lirc/lirc-0.10.0.tar.gz*.
    - As a shortcut, the python package is available in python-pkg/dist
      after a plain *make* in the lirc source tree.


Documentation
-------------

See the main LIRC API docs, look for "Python bindings" under Modules. It
is also possible to run Doxygen in the package doc/ directory to create
some slightly less polished documents for just the python package.

Examples
--------

The python package contains the lirctool script which demonstrates many
aspects of the API. The systemwide LIRC installations has besides
documentation (above) also smaller examples under doc/python-bindings.


See also:
---------

https://github.com/pylover/aiolirc

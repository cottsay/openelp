OpenELP, an Open Source EchoLink Proxy
======================================

OpenELP is an open source EchoLink proxy for Linux and Windows. It aims to be
efficient and maintain a small footprint, while still implementing all of the
features present in the official EchoLink proxy.

OpenELP also has the ability to bind to multiple network interfaces which are
routed to unique external IP addresses, and therefore is capable of accepting
connections from multiple clients simultaneously.

Prerequisites
-------------
To build OpenELP you will need:
* [CMake](https://cmake.org/)
* [GCC](https://gcc.gnu.org/) or [Visual Studio](aka.ms/vs2015)
* [PCRE2](http://www.pcre.org/) or [SVN](https://subversion.apache.org/)

If your system doesn't have PCRE2 development files installed, you have the
option of bundling PCRE2 with OpenELP. To do this, you must have SVN installed,
and specify `-DOPENELP_BUNDLE_PCRE:BOOL=ON` when you call `cmake`.

To create a Windows installer, you will also need to install
[NSIS](http://nsis.sourceforge.net/)

The only runtime dependency that OpenELP has is on the PCRE2 shared library,
unless PCRE2 was bundled into OpenELP.

Compiling
---------
Linux:

    mkdir build && cd build
    cmake ..
    make

Windows:

    mkdir build && cd build
    cmake .. -DOPENELP_BUNDLE_PCRE:BOOL=ON
    devenv openelp.sln /build

Windows Installer:

    devenv openelp.sln /project PACKAGE /build

License
-------
See [LICENSE](./LICENSE) file.

Bugs
----
All issues and feature requests should be directed to
[the bug tracker](https://github.com/cottsay/openelp/issues). Please review any
open issues before filing new ones.


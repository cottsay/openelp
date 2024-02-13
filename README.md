OpenELP, an Open Source EchoLink Proxy
======================================

OpenELP is an open source EchoLink proxy for Linux and Windows. It aims to be
efficient and maintain a small footprint, while still implementing all of the
features present in the official EchoLink proxy.

OpenELP also has the ability to bind to multiple network interfaces which are
routed to unique external IP addresses, and therefore is capable of accepting
connections from multiple clients simultaneously.

[![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/cottsay/openelp/ci.yaml?branch=main&event=push&logo=github)](https://github.com/cottsay/openelp/actions/workflows/ci.yaml?query=branch%3Amain+event%3Apush) [![AUR version](https://img.shields.io/aur/version/openelp?logo=arch%20linux&&logoColor=ffffff)](https://aur.archlinux.org/packages/openelp/) [![EPEL package](https://img.shields.io/fedora/v/openelp/epel8?label=epel&logo=red%20hat)](https://src.fedoraproject.org/rpms/openelp) [![Fedora package](https://img.shields.io/fedora/v/openelp?logo=fedora)](https://src.fedoraproject.org/rpms/openelp) [![Codecov](https://img.shields.io/codecov/c/github/cottsay/openelp/main?logo=codecov&logoColor=ffffff)](https://app.codecov.io/gh/cottsay/openelp/branch/main)

Prerequisites
-------------
To build OpenELP you will need:
* [CMake](https://cmake.org/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [GCC](https://gcc.gnu.org/) or [Visual Studio](http://aka.ms/vs2015)

If available, OpenELP can use:
* [PCRE2](http://www.pcre.org/)
* [OpenSSL](https://www.openssl.org/)
* [Doxygen](http://www.doxygen.org/)

If your system doesn't have PCRE2 development files installed, you have the
option of bundling PCRE2 with OpenELP. To do this, specify
`-DOPENELP_BUNDLE_PCRE:BOOL=ON` when you call `cmake`. CMake will download
the PCRE2 sources automatically and build them into the OpenELP library.

To create a Windows installer, you will also need to install
[NSIS](http://nsis.sourceforge.net/)

The only runtime dependency that OpenELP has is on the PCRE2 shared library,
unless PCRE2 was bundled into OpenELP.

To install these prerequisites on Fedora and RHEL (w/EPEL), run:
```
sudo dnf install cmake doxygen gcc pcre2-devel pkgconfig openssl-devel
```

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

EchoLink&reg; is a registered trademark of Synergenics, LLC.

Bugs
----
All issues and feature requests should be directed to
[the bug tracker](https://github.com/cottsay/openelp/issues). Please review any
open issues before filing new ones.


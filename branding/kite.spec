%define _unpackaged_files_terminate_build 0

%bcond_with toolchain_clang
%bcond_with toolchain_gcc

%if %{with toolchain_gcc}
%global toolchain gcc
%else
%global toolchain clang
%endif

# Change these variables if you want to use custom keys
# Leave blank if you want to build Kite without an MSA ID or CurseForge API key
%global msa_id default
%global curseforge_key default

# Set the Qt version
%global qt_version 6
%global min_qt_version 6.8

# Give the launcher our build platform
%global build_platform unknown

%if 0%{?fedora}
%global build_platform Fedora
%endif

%if 0%{?rhel}
%global build_platform RedHat
%endif

%if 0%{?centos}
%global build_platform CentOS
%endif

%global version v10.0.0

Name:           kite
Version:        %{version}
Release:        1%{?dist}
Summary:        Custom Minecraft launcher with multi-instance management
Group:          Amusements/Games
License:        Apache-2.0 AND LGPL-3.0-only AND LGPL-2.1 AND OFL-1.1 AND MIT
URL:            https://github.com/grxtor/kite
Source0:        https://github.com/grxtor/kite/releases/download/%{version}/Kite-%{version}.tar.gz

%if "%{toolchain}" == "gcc"
BuildRequires:    gcc-c++
%endif
%if "%{toolchain}" == "clang"
BuildRequires:    clang
BuildRequires:    lld
%endif

%if 0%{?fedora} > 41
BuildRequires:    temurin-17-jdk
%else
BuildRequires:    java-17-openjdk-devel
%endif

BuildRequires:    cmake >= 3.22
BuildRequires:    ninja-build
BuildRequires:    extra-cmake-modules

BuildRequires:    rust
BuildRequires:    cargo
BuildRequires:    pkgconfig(gpg-error) >= 1.36
BuildRequires:    libassuan-devel
BuildRequires:    libgpg-error-devel

BuildRequires:    gpgme-devel
BuildRequires:    gpgmepp-devel

BuildRequires:    cmake(VulkanHeaders)
BuildRequires:    pkgconfig(libarchive)
BuildRequires:    pkgconfig(libcmark)
# https://bugzilla.redhat.com/show_bug.cgi?id=2166815
# Fedora versions < 38 (and thus RHEL < 10) don't contain cmark's binary target
# We need that
%if 0%{?fedora} && 0%{?fedora} < 38 || 0%{?rhel} && 0%{?rhel} < 10
BuildRequires:    cmark
%endif
BuildRequires:    pkgconfig(libqrencode)
BuildRequires:    pkgconfig(scdoc)
BuildRequires:    pkgconfig(tomlplusplus)
BuildRequires:    pkgconfig(zlib)

BuildRequires:    git-core

BuildRequires:    cmake(Qt%{qt_version}Concurrent) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Core) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}CoreTools) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Network) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}NetworkAuth) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}OpenGL) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Test) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Widgets) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Xml) >= %{min_qt_version}

BuildRequires:    desktop-file-utils
BuildRequires:    libappstream-glib

Requires:         qt%{qt_version}-qtimageformats
Requires:         qt%{qt_version}-qtsvg

Requires:         javapackages-filesystem
Recommends:       java-25-openjdk
Recommends:       java-21-openjdk
# See note above
%if 0%{?fedora} && 0%{?fedora} < 42
Recommends:       java-17-openjdk
Suggests:         java-1.8.0-openjdk
%endif

# Used to gather GPU with `lspci`
Requires:         pciutils
# Needed for LWJGL [2.9.2, 3) https://github.com/LWJGL/lwjgl/issues/128
Recommends:       xrandr
# Needed for using narrator in minecraft
Recommends:       flite

%description
A custom launcher for Minecraft that allows you to easily manage
multiple installations of Minecraft at once (Fork of MultiMC)

%prep
%autosetup -n Kite-%{version} -p1

%build

cd "%{_builddir}/Kite-%{version}"

%cmake -G Ninja \
    %if "%{toolchain}" == "clang"
    -D CMAKE_LINKER_TYPE=LLD \
    %endif
    -D Kite_QT_VERSION_MAJOR="%{qt_version}" \
    -D Kite_BUILD_PLATFORM="%{build_platform}" \
    -D Kite_PLUGINS=ON \
    -D Kite_UNLEASH_APP_NAME="Linux Native" \
    -D Kite_STAGING_PLUGINS=ON \
    -W no-dev \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    %if 0%{?fedora} < 41
    -D Kite_DISABLE_JAVA_DOWNLOADER=ON \
    %endif
    %if "%{msa_id}" != "default"
    -D Kite_MICROSOFT_CLIENT_ID="%{msa_id}" \
    %endif
    %if "%{curseforge_key}" != "default"
    -D Kite_CURSEFORGE_API_KEY="%{curseforge_key}" \
    %endif
%cmake_build

%install

cd "%{_builddir}/Kite-%{version}"

%cmake_install

%check

cd "%{_builddir}/Kite-%{version}"

%ctest
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.grxtor.Kite.desktop
%if 0%{?fedora} > 37 || 0%{?rhel} > 9
appstream-util validate-relax --nonet \
  %{buildroot}%{_metainfodir}/io.github.grxtor.Kite.metainfo.xml
%endif

%files
%license COPYING.md
%doc README.md
%{_bindir}/kite
%{_bindir}/kite-crashreporter
%{_datadir}/applications/io.github.grxtor.Kite.desktop
%{_datadir}/metainfo/io.github.grxtor.Kite.metainfo.xml
%{_datadir}/mime/packages/io.github.grxtor.Kite.xml
%{_datadir}/icons/hicolor/*/apps/io.github.grxtor.Kite.*
%{_datadir}/Kite/
%{_libdir}/libiconfix.so
%{_libdir}/libnbt++.so.3*
%{_libdir}/libnbt++.so
%{_libdir}/librainbow.so
%{_mandir}/man6/*
%{_datadir}/Kite/JavaCheck.jar
%{_datadir}/Kite/NewLaunch.jar
%{_datadir}/Kite/mmcmodules/

%package devel
Summary: Development files for Kite
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Development files for building against Kite and its bundled libraries.

%files devel
%{_includedir}/kite-sdk/
%{_prefix}/lib/cmake/Kite_SDK/

%changelog
* Mon Apr 20 2026 Mehmet Samet Duman <yongdohyun@projecttick.org> - %{version}-1
- Initial Fedora draft

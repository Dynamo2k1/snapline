Name:           snapline
Version:        0.1.0
Release:        1%{?dist}
Summary:        Region screenshot tool with annotation and upload
License:        GPL-3.0-or-later
URL:            https://github.com/Dynamo2k1/snapline
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc-c++
BuildRequires:  cmake(Qt6Widgets) >= 6.4
BuildRequires:  cmake(Qt6Svg)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6PrintSupport)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-xfixes)
BuildRequires:  pkgconfig(x11)
BuildRequires:  desktop-file-utils

%if 0%{?fedora}
Requires:       qt6-qtsvg
%endif
Recommends:     wl-clipboard
Recommends:     xdg-desktop-portal
Recommends:     optipng
Suggests:       grim

%description
Press a hotkey, drag over the part of the screen you want, mark it up with
pen, line, arrow, rectangle, circle, marker, text and blur tools, then copy
it, save it, print it or upload it to Imgur or your own server. Works on X11
and Wayland (GNOME, KDE, wlroots compositors).

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DBUILD_TESTING=ON
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.snapline.Snapline.desktop
QT_QPA_PLATFORM=offscreen %ctest

%files
%license LICENSE
%doc README.md
%{_bindir}/snapline
%{_datadir}/applications/io.github.snapline.Snapline.desktop
%{_datadir}/metainfo/io.github.snapline.Snapline.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.snapline.Snapline.svg

%changelog
* Sat Oct 17 2026 Snapline Maintainers <maintainers@snapline.invalid> - 0.1.0-1
- Initial release

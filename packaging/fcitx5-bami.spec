Name:           fcitx5-bami
Version:        0.1.0
Release:        1%{?dist}
Summary:        Vietnamese input method for Fcitx 5
License:        MIT
URL:            https://github.com/crazyads69/fcitx5-bami
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++
BuildRequires:  golang >= 1.18
BuildRequires:  extra-cmake-modules
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(enchant-2)
BuildRequires:  hunspell-en-US
BuildRequires:  fcitx5-devel >= 5.0.14
Requires:       fcitx5 >= 5.0.14
Requires:       hunspell-en-US
Requires:       glib2
Conflicts:      fcitx5-bamboo

%description
Bami types Vietnamese (Telex variants, VNI, VIQR, MS layout). It edits the
application text through surrounding-text only where that is provably safe and
uses preedit elsewhere. It embeds a Go runtime and cannot be loaded together
with fcitx5-bamboo.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%check
%ctest

%install
%cmake_install

%files
%license LICENSE THIRD_PARTY_NOTICES.md
%{_bindir}/fcitx5-bami-init
%{_datadir}/systemd/user/fcitx5-bami-init.service
%{_datadir}/fcitx5/bami/profiles/bami-panel.conf
%{_libdir}/fcitx5/libbami.so
%{_datadir}/fcitx5/addon/bami.conf
%{_datadir}/fcitx5/inputmethod/bami.conf

%changelog
* Sat Oct 31 2026 crazyads69 <leminhtri11a1@gmail.com> - 0.1.0-1
- Initial package

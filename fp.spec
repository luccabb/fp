%global debug_package %{nil}

Name:           fp
Version:        1.0.0
Release:        1%{?dist}
Summary:        Find free ports via kernel socket binding
License:        MIT
URL:            https://github.com/luccabb/fp
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
fp finds free network ports using the kernel's own port allocator
(bind to port 0). The OS guarantees the returned port is not in use
and not in TIME_WAIT. Supports TCP/UDP, IPv4/IPv6, port ranges,
and batch requests with guaranteed uniqueness.

%prep
%autosetup

%build
%make_build

%install
%make_install PREFIX=%{_prefix}

%check
make test

%files
%license LICENSE
%{_bindir}/fp

%changelog
* Sun Feb 09 2025 luccabb <luccabb@users.noreply.github.com> - 1.0.0-1
- Initial package

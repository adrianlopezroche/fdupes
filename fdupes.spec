# fdupes spec file
%define            _topdir    %(echo $PWD)/
%define            _appdir    /usr/local/bin
%define            major      1.x
%define            version    %(cat VERSION)
%define            release    %(date +%s)
%define            buildroot  %{_topdir}/BUILD

Name:               fdupes
Version:           %{version}
Release:           %{release}
Summary:           FDUPES is a program for identifying or deleting duplicate files residing within specified directories.

License:           GPL 2.0
URL:               https://github.com/adrianlopezroche/fdupes

BuildRoot:         %{buildroot}/%{major}
BuildArch:         x86_64
# Requires:

%description
FDUPES is a program for identifying or deleting duplicate files residing within specified directories.

%prep
mkdir -vp    %{buildroot}/ %{_topdir}/{RPMS,SRPMS}
rm -rf      %{buildroot}/*

%build
mkdir -p    %{buildroot}/
cp -r       %{_topdir}/SRC/%{major}/root/* %{buildroot}/


%files
%defattr(-,root,root,-)
/usr/local/bin/fdupes
%doc
/usr/local/man/man1/fdupes.1

%changelog


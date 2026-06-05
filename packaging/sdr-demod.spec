%global debug_package %{nil}
Name:       sdr-demod
Version:    %{pkg_version}
Release:    1%{?dist}
Summary:    OpenRFStack multi-protocol demodulator
License:    Proprietary
URL:        https://github.com/OpenRFStack/DemodApp
BuildArch:  x86_64
AutoReqProv: no
Requires:   qpid-proton-cpp tinyxml2 liquid-dsp spdlog fmt

%description
DemodApp decodes 25+ RF protocols from IQ data published by AcquisitionApp.
Protocols: ADS-B, AIS, P25 Phase 1/2, EAS/SAME, DSC, ACARS, VDL2, FM/AM,
PSK31, NAVTEX, DTMF, D-STAR, DMR, TETRA. Includes content-layer threat
validators for spoofing and rogue-site detection.

%prep
%build
%install

%files
/usr/bin/sdr_demod

%changelog
* Thu Jan 01 2026 OpenRFStack CI <noreply@github.com> - %{pkg_version}-1
- Automated build from main/1.0

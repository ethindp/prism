# Security Policy

Prism treats the security of its software and its users with the utmost seriousness. This document sets out the procedure for reporting a security vulnerability and the criteria by which such reports are evaluated. Reporters should be aware that this policy is applied more strictly than the project's contributing guidelines, and that a report may be declined on any of the grounds enumerated below. Please read this policy in full before submitting a report.

## Supported Versions

Prism accepts security reports against the three most recent minor releases. Releases predating this range are unsupported, and vulnerabilities affecting only unsupported releases will not be addressed. Prism does not presently operate a long-term support (LTS) programme. This may change in future, and any such change will be reflected in this document.

## Reporting a Vulnerability

All vulnerabilities must be reported through GitHub's private vulnerability reporting mechanism. Under no circumstances should a vulnerability be disclosed through a public issue tracker, discussion forum, pull request, or any other public venue prior to coordinated disclosure.

A report should provide as much detail as the reporter is able to supply, including the information ordinarily expected of a standard issue. A reliable reproduction is of particular importance; a report that cannot be reproduced is substantially more difficult to act upon and may be declined on that basis alone.

Reporters may, at their discretion, include a proposed remediation. Where no remediation is supplied, resolution may require extended discussion and coordination with the reporter.

## Grounds for Rejection

A report may be declined, without further action, on any of the following grounds.

* The vulnerability is purely theoretical or is not practically exploitable. This ground does NOT apply to a defect demonstrated to constitute undefined behavior, as defined by ISO/IEC 14882:2024 or, where the defect is observed at or through Prism's C interface, ISO/IEC 9899:2024, including memory corruption arising therefrom (such as an out-of-bounds access, a use-after-free, or a data race). Such a defect is accepted upon a demonstration of soundness, irrespective of whether a practical exploit has been produced, provided that:
    1. the reporter substantiates that the behavior falls within the foregoing category as that category is defined by the referenced standard; and
    2. the behavior does not result solely from the immediate caller's breach of a precondition of Prism's public API, meaning a precondition expressed on or documented with the public declaration the caller compiles against. A report is not excluded on this ground where every such documented public precondition was honored and the behavior remains reachable through data or state supplied to Prism, including where an internal assumption that does not form part of the public contract (such as an `[[assume]]` expression or an internal invariant) is thereby violated. The mere presence of an annotation does not, by itself, exclude a report.
* The vulnerability depends upon preconditions that themselves constitute a compromise equal to or greater than that which the vulnerability would confer. This includes, without limitation, any report presuming that the attacker already possesses full control of the host or the ability to execute arbitrary code within the affected process.
* The vulnerability requires physical access to the host, or a level of privilege that Prism's threat model does not grant to an attacker.
* The defect resides in a third-party dependency, speech backend, screen reader, or operating-system facility, rather than in Prism itself. Such issues must be reported to the relevant upstream project.
* The report describes a missing hardening measure or defence-in-depth improvement that is not, in isolation, exploitable. Suggestions of this kind are welcome as ordinary issues but will not be treated as vulnerabilities.
* The vulnerability arises solely from use of Prism in a manner expressly documented as unsupported or unsafe.
* The issue is already known to the maintainers, has already been reported, or is already public.
* The report consists of unverified automated tooling or scanner output submitted without accompanying independent analysis.
* The vulnerability was discovered by an artificial intelligence system or model and has not been independently validated and verified by a security researcher.

Persistent submission of reports falling within the foregoing categories, after the reporter has been requested to desist, will result in all further reports from that reporter being disregarded.

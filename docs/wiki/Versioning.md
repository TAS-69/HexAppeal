# Release versioning

**Public releases start at 1.0.0.** `VERSION` is the public package number. `DEV_VERSION` is independent and identifies development builds.

Ordinary edits, commits and tests do not advance the public version. Packaging reads the requested number and never increments it. I assign a new public number only when requesting another public build.

**1.0.0 was refreshed on 17 November 2026**, replacing the initial split downloads at the same release URL. It includes the latest checked 0.5.8 development changes, the combined player package and expanded generic SDK examples. The earlier release files and tag were backed up before replacement. Release notes and checksums identify this replacement explicitly.

The player download is one archive containing the runtime, body routing and default outfit refits. DMM uses one Mods entry and its normal ASI switch. Development modules remain separate. Source and SDK archives are optional creator resources.

Components in a public package use the same release number. SDK/API table versions describe compatibility and are independent of the download version. A development number is not a public upgrade path.

Tags use `v1.0.0` style. Future packages receive a new number when requested; a deliberate replacement must be identified in its notes and preserve the previous files. The verification summary records the actual binary hash, checks and live-test limits.

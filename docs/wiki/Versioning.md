# Release versioning

**The first Nexus release is version 1.0.0.** `VERSION` stays at that number during preparation, including the explicitly requested core-only package. Previous private release files and tags are backed up before replacement; release notes identify the current scope and hashes.

`DEV_VERSION` is independent. Ordinary edits, commits and development builds do not advance the public number. After the Nexus release, I assign a new public number only when requesting another public package. Packaging never increments it automatically.

The player download is one archive containing the runtime, body routing and default outfit refits. Source and SDK archives are optional creator resources. API table versions describe SDK compatibility independently of either build number.

Tags use `v1.0.0` style. Checksums and verification summaries identify the exact files; a release number alone cannot distinguish deliberate pre-release replacements.

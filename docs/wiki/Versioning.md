# Release versioning

**The first Nexus release is 1.0.0. The requested body-choice update is 1.0.1.** Existing published downloads and tags stay intact. Release notes identify the scope and hashes.

`DEV_VERSION` is independent. Ordinary edits, commits and development builds do not advance the public number. After the Nexus release, I assign a new public number only when requesting another public package. Packaging never increments it automatically.

The player download is one archive containing the runtime, body routing and default outfit refits. Source and SDK archives are optional creator resources. API table versions describe SDK compatibility independently of either build number.

Tags use `v1.0.0` style. Checksums and verification summaries identify the exact files; a release number alone cannot distinguish deliberate pre-release replacements.

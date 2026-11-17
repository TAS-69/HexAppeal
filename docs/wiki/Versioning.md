# Release versioning

**Public releases start at 1.0.0.** `VERSION` is the public package number. `DEV_VERSION` is independent and identifies development builds.

Ordinary edits, commits, tests and development builds do not advance the public version. Packaging reads the requested public number; it never increments it. A new public number is assigned only when I explicitly request another public build to be packaged.

The next public player download is one archive containing the runtime, body routing and default garment refits under one Hex Appeal folder and manifest. DMM uses one Mods entry and its normal ASI switch. Development modules remain separate. Release packaging requires checked manager import and deployment. Source and SDK archives are optional creator resources. This does not replace the already published split 1.0.0 packages.

All components in one public release use that release number. SDK/API table versions describe compatibility and are independent of the download version. A development build number is not a public upgrade path.

Public tags use `v1.0.0` style. A published tag or archive is not silently replaced with different code. Release notes identify the actual binary hash, checks and known limits. The existing development lineage remains separate from this first public package.

# Verification: 1.0.2

**Prepared 18 November 2026.** Corrected DMM import layout and explicit package/ASI version metadata. No shaping, Wardrobe or game-resource changes.

The accepted layout was imported twice through DMM 5.0.1. All 131 installed candidate files matched. VENUS and Vanilla remained in one group. Both options then mounted independently: 46 VENUS resources, 30 Vanilla resources, seven appearance routes per option, primary/secondary registrations and no opposite-option routes. The armour pack kept its shared boots. Settings and unrelated active mod choices were preserved.

DMM retained the old 1.0.0 installation beside the new one. Obsolete copies were backed up and removed, leaving one current ASI. The installation guide therefore requires removing the previous package before installing 1.0.2, while retaining saved settings.

The rebuilt 1.0.2 runtime receives its own offline, native-calculation and startup checks. Historical fitting checks retain their original binaries and scope in the [1.0.1 record](VERIFICATION-1.0.1.md). A new version label does not certify another full all-witch/movement/LOD matrix. External outfit assets still need their own rendering acceptance.

The final package audit checks the corrected layout, current runtime and unchanged game resources. A fresh Nexus download of the replacement has not been tested. No downloaded bodies, presets, outfits, upstream plugins, private permissions or development executables are distributed. Game-derived resources and licensed adaptations retain their separate rights and notices.

## Current rebuild checks

Runtime SHA-256: `104fbe304ce3392389732061d26ad4e84abd8ec7c9429413c8218c36f2fe0ee7`. All **22 offline checks**, including Windows FileVersion 1.0.2, passed; the SDK generator passed 36 cases and helper tests passed 38. Isolated native checks covered all nine combinations, 36 weighted Height/Weight extremes, eight weighted baseline/FLAT cases without HEX and eleven matching reference poses. No runtime source or API functions changed.

After importing the 1.0.2 test archive, the player confirmed the DMM version labels and successful world/F9 startup. The deployed and sole staged plugin matched this rebuild, current-version initialization and panel rendering were logged, and all four settings files remained byte-identical. This is a startup check, not a repeated visual fitting matrix. Updated final guides/verification are audited separately from the imported candidate.

from pathlib import Path
import json,sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from release_policy import manager_acceptance_matches_runtime,validate_runtime
from package_character_addon import character_addon_entries
from release_version import METADATA,check_version_metadata,release_version,set_release_version,validate_version

class ReleasePolicy(unittest.TestCase):
    def test_release_version_updates_all_source_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for relative,keys in METADATA:
                path=root/relative
                path.parent.mkdir(parents=True,exist_ok=True)
                row={"version":"1.0.4","title":"Existing title"}
                path.write_text(json.dumps({"modinfo":row} if keys else row),encoding="utf-8")
            (root/"VERSION").write_text("1.0.4\n",encoding="ascii")
            set_release_version(root,"1.0.5")
            self.assertEqual(release_version(root),"1.0.5")
            check_version_metadata(root,"1.0.5")
            for relative,_ in METADATA:
                self.assertIn("Existing title",(root/relative).read_text(encoding="utf-8"))
            with self.assertRaises(ValueError):
                check_version_metadata(root,"1.0.6")
            for invalid in ("1.0","1.0.06","1.0.65536","1.0.5-preview"):
                with self.subTest(invalid=invalid),self.assertRaises(ValueError):
                    validate_version(invalid)

    def test_optional_addon_has_an_independent_release_version(self):
        root=Path(__file__).resolve().parents[2]
        addon=root/"addons/maegu-woosa"
        route,entries=character_addon_entries(addon/"Characters/maegu-woosa.tsv",addon/"README.md",
                                            "Hex Appeal - Maegu and Woosa",
                                            (addon/"VERSION").read_text(encoding="ascii").strip(),
                                            license_file=root/"LICENSE")
        self.assertEqual(json.loads(entries["mod.json"])["modinfo"]["version"],"1.0.0")
        self.assertEqual(set(entries),{"mod.json","README.md","LICENSE","files/0009/"+route})
        self.assertEqual(entries["LICENSE"],(root/"LICENSE").read_bytes())
        self.assertIn(b"#patch\tHex Appeal - Maegu and Woosa\t1.0.0\t1.0.5",entries["files/0009/"+route])
        self.assertIn(b"NHW_Unique_Maegu_3452",entries["files/0009/"+route])
        self.assertIn(b"NHW_Unique_Woosa_3453",entries["files/0009/"+route])
    def test_removed_integration_is_rejected_in_both_binary_string_encodings(self):
        for encoding in ('utf-8','utf-16le'):
            with self.subTest(encoding=encoding),self.assertRaises(ValueError):validate_runtime('cd_phw_00_ub_00_0191_u'.encode(encoding))
    def test_legacy_settings_key_is_not_an_outfit_integration(self):
        validate_runtime(b'DancerColour\0DancerSource\0')

    def test_manager_acceptance_requires_a_fresh_import_of_the_same_runtime(self):
        acceptance={'verified':True,'final_zip_freshly_imported':True}
        report={'candidate_installed_files_exact_match':True,'public_runtime_sha256':'test-runtime'}
        self.assertTrue(manager_acceptance_matches_runtime(acceptance,report,'test-runtime'))
        self.assertFalse(manager_acceptance_matches_runtime(acceptance,report,'different-runtime'))
        self.assertFalse(manager_acceptance_matches_runtime(
            {**acceptance,'final_zip_freshly_imported':False},report,'test-runtime'))
        self.assertFalse(manager_acceptance_matches_runtime(
            acceptance,{**report,'candidate_installed_files_exact_match':False},'test-runtime'))

if __name__=='__main__':unittest.main()

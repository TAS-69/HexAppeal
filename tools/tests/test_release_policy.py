from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from release_policy import validate_runtime

class ReleasePolicy(unittest.TestCase):
    def test_removed_integration_is_rejected_in_both_binary_string_encodings(self):
        for encoding in ('utf-8','utf-16le'):
            with self.subTest(encoding=encoding),self.assertRaises(ValueError):validate_runtime('cd_phw_00_ub_00_0191_u'.encode(encoding))
    def test_legacy_settings_key_is_not_an_outfit_integration(self):
        validate_runtime(b'DancerColour\0DancerSource\0')

if __name__=='__main__':unittest.main()

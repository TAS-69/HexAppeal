from pathlib import Path
import hashlib,json,sys,unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from player_package import player_package

class PlayerPackage(unittest.TestCase):
    def package(self,**overrides):
        options={'version':'0.5.7-development','body_files':[('files/0009/character/witch.app_xml.patch',b'patch')],
                 'refit_files':[('files/0009/character/default.pac',b'own generated garment')],
                 'runtime_files':[('HexAppeal.asi',b'own runtime'),('HexAppeal.ini',b'settings')],
                 'documents':[('LICENSE',b'licence')],
                 'allowed_pacs':{'files/0009/character/default.pac':hashlib.sha256(b'own generated garment').hexdigest()},
                 'dependency_hashes':{hashlib.sha256(b'external body').hexdigest()}}
        options.update(overrides)
        return dict(player_package(**options))

    def test_player_archive_has_one_root_and_one_manifest(self):
        entries=self.package()
        self.assertEqual({name.split('/')[0] for name in entries},{'Hex Appeal'})
        self.assertEqual([name for name in entries if name.endswith('/mod.json')],['Hex Appeal/mod.json'])
        self.assertEqual(json.loads(entries['Hex Appeal/mod.json'])['modinfo']['title'],'Hex Appeal')
        self.assertIn('Hex Appeal/HexAppeal.asi',entries)
        self.assertIn('Hex Appeal/files/0009/character/default.pac',entries)

    def test_body_and_refit_overlap_cannot_silently_replace_files(self):
        with self.assertRaises(ValueError):self.package(body_files=[('files/0009/character/default.pac',b'own generated garment')])

    def test_dependency_content_is_rejected_even_when_renamed(self):
        with self.assertRaises(ValueError):self.package(documents=[('notes.txt',b'external body')])

    def test_traversal_case_collisions_and_foreign_plugins_are_rejected(self):
        for name in ('../outside.txt','/outside.txt','C:/outside.txt','files\\outside.txt','hexappeal.ini','Other.asi','helper.exe'):
            with self.subTest(name=name),self.assertRaises(ValueError):self.package(documents=[(name,b'x')])

    def test_missing_runtime_or_garments_is_not_a_complete_package(self):
        with self.assertRaises(ValueError):self.package(runtime_files=[('HexAppeal.asi',b'own runtime')])
        with self.assertRaises(ValueError):self.package(refit_files=[])

if __name__=='__main__':unittest.main()

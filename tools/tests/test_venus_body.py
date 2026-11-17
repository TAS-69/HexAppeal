import json,sys,tempfile,unittest
from pathlib import Path

root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from venus_body import BODY_PATH,active_body,reference_hashes,refit_reference

class VenusBodies(unittest.TestCase):
    def setUp(self):
        self.metadata=next(item for item in json.loads((root/'dependencies.json').read_text(encoding='utf-8'))['required'] if item['id']=='venus')
        self.temporary=tempfile.TemporaryDirectory(prefix='venus-variants-');self.addCleanup(self.temporary.cleanup)
        self.base=Path(self.temporary.name)

    def test_both_reference_variants_share_the_declared_refit_baseline(self):
        self.assertEqual({body['variant'] for body in self.metadata['verified_variants']},{'Human Body','Elf Body'})
        self.assertEqual(len(reference_hashes(self.metadata)),2)
        for digest in reference_hashes(self.metadata):self.assertEqual(refit_reference(self.metadata,digest),self.metadata['sha256'])
        with self.assertRaises(ValueError):refit_reference(self.metadata,'not a checked geometry reference')

    def test_human_body_alone_is_a_valid_active_choice(self):
        selected='Venus 2.6/[01] Human Body'
        name,path=active_body({'activeBrowserMods':[selected]},self.base/'config.json')
        self.assertEqual(name,selected);self.assertEqual(path,self.base/'mods'/selected/BODY_PATH)

    def test_elf_and_windows_separators_with_separate_storage(self):
        storage=self.base/'another storage'
        name,path=active_body({'activeBrowserMods':['Venus 2.6\\[02] Elf Body'],'modsPath':str(storage)},self.base/'config.json')
        self.assertEqual(name,'Venus 2.6/[02] Elf Body');self.assertEqual(path,storage/name/BODY_PATH)

    def test_missing_or_both_variants_are_an_installation_error(self):
        for names in ([],['Venus 2.6/[01] Human Body','Venus 2.6/[02] Elf Body']):
            with self.subTest(names=names),self.assertRaises(ValueError):active_body({'activeBrowserMods':names},self.base/'config.json')

    def test_active_mod_cannot_escape_storage(self):
        with self.assertRaises(ValueError):active_body({'activeBrowserMods':['Venus/../../elsewhere/[01] Human Body']},self.base/'config.json')

if __name__=='__main__':unittest.main()

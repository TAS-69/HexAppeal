from pathlib import Path
import hashlib,json,sys,unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from player_package import WARDROBE_INTERACTION_FILE,player_package,player_choices

MAIN_DESCRIPTION = 'Applies the Venus body to every witch in Crimson Desert. Because even dark magic deserves better curves.'
INTERACTION_RULE = (Path(__file__).resolve().parents[2]/'build-data'/WARDROBE_INTERACTION_FILE).read_bytes()

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
        metadata=json.loads(entries['Hex Appeal/mod.json'])['modinfo']
        self.assertEqual(metadata['title'],'Hex Appeal')
        self.assertEqual(metadata['description'],MAIN_DESCRIPTION)
        self.assertIn('Hex Appeal/HexAppeal.asi',entries)
        self.assertIn('Hex Appeal/files/0009/character/default.pac',entries)

    def test_body_and_refit_overlap_cannot_silently_replace_files(self):
        with self.assertRaises(ValueError):self.package(body_files=[('files/0009/character/default.pac',b'own generated garment')])

    def test_reports_cannot_be_imported_as_unknown_patches(self):
        for name in ('VERIFICATION.json','docs/report.JSON'):
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'extra JSON documents'):self.package(documents=[(name,b'{}')])
        self.assertIn('Hex Appeal/VERIFICATION.txt',self.package(documents=[('VERIFICATION.txt',b'{}')]))

    def test_dependency_content_is_rejected_even_when_renamed(self):
        with self.assertRaises(ValueError):self.package(documents=[('notes.txt',b'external body')])

    def test_traversal_case_collisions_and_foreign_plugins_are_rejected(self):
        for name in ('../outside.txt','/outside.txt','C:/outside.txt','files\\outside.txt','hexappeal.ini','Other.asi','helper.exe'):
            with self.subTest(name=name),self.assertRaises(ValueError):self.package(documents=[(name,b'x')])

    def test_missing_runtime_or_garments_is_not_a_complete_package(self):
        with self.assertRaises(ValueError):self.package(runtime_files=[('HexAppeal.asi',b'own runtime')])
        with self.assertRaises(ValueError):self.package(refit_files=[])

    def test_game_metadata_requires_an_exact_allowlist_and_complete_output(self):
        name='files/0009/character/bin__/partprefabtable.pappt'
        data=b'three additive registrations'
        resources={name:hashlib.sha256(data).hexdigest()}
        with self.assertRaisesRegex(ValueError,'Unexpected game metadata'):
            self.package(documents=[(name,data)])
        self.assertIn('Hex Appeal/'+name,self.package(documents=[(name,data)],allowed_resources=resources))
        with self.assertRaisesRegex(ValueError,'Unexpected game metadata'):
            self.package(documents=[(name,b'changed')],allowed_resources=resources)
        with self.assertRaisesRegex(ValueError,'resource output set is incomplete'):
            self.package(allowed_resources=resources)
        for suffix in ('.prefab','.pac_xml','.dds'):
            with self.assertRaisesRegex(ValueError,'Unexpected game metadata'):
                self.package(documents=[('files/0009/foreign'+suffix,b'unknown')])

    def test_outfit_setup_and_resource_routes_are_excluded(self):
        for name in ('Setup Dancer.cmd','tools/prepare_dancer_dependencies.py','files/0009/character/model/cd_phw_00_ub_00_0191.pac'):
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'Excluded outfit integration'):self.package(documents=[(name,b'not distributed')])

    def test_two_body_choices_share_one_runtime_and_keep_assets_separate(self):
        def variant(data):
            name='files/0009/character/own.pac'
            return {'files':[(name,data)],'models':{name:hashlib.sha256(data).hexdigest()},'resources':{}}
        documents=[('LICENSE',b'licence'),('licenses/upstream.txt',b'upstream notice'),('docs/Guide.md',b'[Help](Help.md#install) ![Logo](assets/logo.svg)'),('docs/Help.md',b'Help'),('docs/assets/logo.svg',b'<svg/>')]
        entries=dict(player_choices('0.5.14-development',{'Venus':variant(b'own Venus refit'),'Vanilla':variant(b'original game body')},[('HexAppeal.asi',b'own runtime'),('HexAppeal.ini',b'settings')],documents,set(),INTERACTION_RULE))
        self.assertEqual([name for name in entries if name.endswith('.asi')],['Hex Appeal/HexAppeal.asi'])
        self.assertEqual(entries['Hex Appeal/PICK ONE - [01] VENUS/0009/character/own.pac'],b'own Venus refit')
        self.assertEqual(entries['Hex Appeal/PICK ONE - [02] Vanilla/0009/character/own.pac'],b'original game body')
        self.assertEqual(len([name for name in entries if name.endswith('mod.json')]),3)
        self.assertIn('mod.json',entries)
        self.assertNotIn('Hex Appeal/mod.json',entries)
        metadata=json.loads(entries['mod.json'])['modinfo']
        self.assertEqual(metadata['title'],'Hex Appeal')
        self.assertEqual(metadata['description'],MAIN_DESCRIPTION)
        for folder in ('[01] VENUS','[02] Vanilla'):
            prefix=f'Hex Appeal/PICK ONE - {folder}/'
            metadata=json.loads(entries[prefix+'mod.json'])['modinfo']
            self.assertEqual(metadata['title'],'PICK ONE: '+folder)
            rule=json.loads(entries[prefix+WARDROBE_INTERACTION_FILE])
            self.assertEqual(rule.pop('modinfo'),metadata)
            self.assertEqual(rule,json.loads(INTERACTION_RULE))
            registration,=rule['targets']
            self.assertEqual(registration['file'],'interactioninfo.pabgb')
            self.assertEqual([(intent['op'],intent['source_key'],intent['new_key'])
                              for intent in registration['intents']],
                             [('clone_record',1000143,1969427),('clone_record',1000370,1969428)])
        self.assertNotIn('Hex Appeal/'+WARDROBE_INTERACTION_FILE,entries)
        self.assertEqual({name.split('/')[1] for name in entries if name.count('/')>1},{'PICK ONE - [01] VENUS','PICK ONE - [02] Vanilla'})
        self.assertEqual(entries['Hex Appeal/NOTICE-upstream.txt'],b'upstream notice')
        self.assertEqual(entries['Hex Appeal/docs--Guide.md'],b'[Help](docs--Help.md#install) ![Logo](docs--assets--logo.svg)')
        with self.assertRaises(ValueError):
            player_choices('test',{'Vanilla':variant(b'body')},[],[],set(),INTERACTION_RULE)

    def test_flattened_guides_keep_html_images_and_space_containing_links(self):
        variant={'files':[],'models':{},'resources':{}}
        docs=[('docs/Player Guide/Home.md',b'<img src="../assets/logo.png"> [Shape](Shape.md#pad)'),('docs/Player Guide/Shape.md',b'Shape'),('docs/assets/logo.png',b'logo')]
        entries=dict(player_choices('test',{'Venus':variant,'Vanilla':variant},[('HexAppeal.asi',b'runtime'),('HexAppeal.ini',b'config')],docs,set(),INTERACTION_RULE))
        self.assertEqual(entries['Hex Appeal/docs--Player Guide--Home.md'],b'<img src="docs--assets--logo.png"> [Shape](docs--Player%20Guide--Shape.md#pad)')

if __name__=='__main__':unittest.main()

from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
import shutil
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('dancer_dependencies', Path(__file__).resolve().parents[1] / 'prepare_dancer_dependencies.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ExternalDancerBindings(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='ha-links-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source, self.target = self.root / 'source', self.root / 'binding'

        for folder, _ in module.COLOURS.values():
            for virtual in [module.UPPER, module.LOWER, module.PROPERTY, module.TEXTURE]:
                p = self.source / folder / virtual
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(b'PAR fixture' if virtual.endswith('.pac') else b'DDS fixture' if virtual.endswith('.dds') else b'property fixture')

    def test_unknown_readable_version_is_a_warning_and_plan_does_not_write(self):
        report = module.plan(self.source, self.target)
        self.assertTrue(report['warnings'])
        self.assertFalse(report['verified_reference'])
        self.assertEqual(len(report['files']), 4)
        self.assertFalse(self.target.exists())

    def test_source_preserved_and_only_four_links_created(self):
        originals = {p: p.read_bytes() for p in self.source.rglob('*') if p.is_file()}
        report = module.apply(module.plan(self.source, self.target, 'black'))
        self.assertTrue(report['bindings_created'])
        for row in report['files']:
            self.assertTrue(Path(row['target']).samefile(row['link_source']))
        self.assertEqual(originals, {p: p.read_bytes() for p in originals})
        self.assertEqual([p.name for p in self.target.glob('*.json')], ['mod.json'])
        self.assertEqual(json.loads((self.target / 'EXTERNAL-SOURCES.txt').read_text(encoding='utf-8-sig'))['model_source'], 'unmodified external dependency')

    def test_existing_target_is_never_overwritten(self):
        self.target.mkdir()
        sentinel = self.target / 'preserve.txt'; sentinel.write_text('keep')
        with self.assertRaisesRegex(ValueError, 'existing mods'):
            module.plan(self.source, self.target)
        self.assertEqual(sentinel.read_text(encoding='utf-8-sig'), 'keep')

    def test_missing_colour_is_a_path_error(self):
        (self.source / module.COLOURS['emerald'][0] / module.TEXTURE).unlink()
        with self.assertRaisesRegex(ValueError, 'Missing dependency'):
            module.plan(self.source, self.target)
        self.assertFalse(self.target.exists())

    def test_wrong_format_is_rejected(self):
        (self.source / module.COLOURS['white'][0] / module.UPPER).write_bytes(b'not a PAC')
        with self.assertRaisesRegex(ValueError, 'incorrect file format'):
            module.plan(self.source, self.target)

    def test_source_changed_after_plan_is_preserved_without_install(self):
        report = module.plan(self.source, self.target)
        source = Path(report['files'][0]['link_source']); source.write_bytes(b'PAR changed')
        with self.assertRaisesRegex(ValueError, 'changed after planning'):
            module.apply(report)
        self.assertFalse(self.target.exists())
        self.assertEqual(source.read_bytes(), b'PAR changed')

    def test_nested_target_rejected(self):
        with self.assertRaisesRegex(ValueError, 'must be separate'):
            module.plan(self.source, self.source / 'binding')

    def main_package(self):
        self.target.mkdir()
        (self.target/module.PLAYER_MARKER).write_text(module.PLAYER_MARKER_TEXT,encoding='ascii',newline='\n')
        (self.target/'mod.json').write_text(json.dumps({'modinfo':{'title':'Hex Appeal','author':'TAS-69'}}),encoding='utf-8')
        (self.target/'HexAppeal.asi').write_bytes(b'own runtime')
        (self.target/'HexAppeal.ini').write_bytes(b'own settings')
        (self.target/'README.txt').write_bytes(b'preserve player guide')
        return {p:p.read_bytes() for p in self.target.iterdir() if p.is_file()}

    def test_integrated_setup_retains_the_main_manifest_runtime_and_guide(self):
        preserved=self.main_package()
        report=module.apply(module.plan(self.source,self.target,'black',integrated=True))
        self.assertTrue(report['integrated_player_package'])
        self.assertEqual(preserved,{p:p.read_bytes() for p in preserved})
        self.assertEqual([p.name for p in self.target.glob('*.json')],['mod.json'])
        for row in report['files']:self.assertTrue(Path(row['target']).samefile(row['link_source']))
        self.assertTrue((self.target/'DANCER-SETUP.txt').is_file())

    def test_integrated_setup_requires_a_recognized_installed_package(self):
        self.target.mkdir()
        with self.assertRaisesRegex(ValueError,'package marker'):module.plan(self.source,self.target,integrated=True)

    def test_integrated_existing_asset_is_never_replaced(self):
        self.main_package()
        target=self.target/'files/0009'/module.UPPER
        target.parent.mkdir(parents=True);target.write_bytes(b'keep my installed outfit')
        with self.assertRaisesRegex(ValueError,'already exist'):module.plan(self.source,self.target,integrated=True)
        self.assertEqual(target.read_bytes(),b'keep my installed outfit')

    def test_integrated_setup_checks_package_identity_again_before_writing(self):
        self.main_package()
        report=module.plan(self.source,self.target,integrated=True)
        (self.target/'mod.json').write_text(json.dumps({'modinfo':{'title':'Another outfit','author':'Someone else'}}),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'not the combined'):module.apply(report)
        self.assertFalse((self.target/'files').exists())

    def test_failed_link_creation_rolls_back_only_new_links(self):
        preserved=self.main_package()
        report=module.plan(self.source,self.target,integrated=True)
        link=module.os.link
        calls=0
        def interrupted(source,target):
            nonlocal calls
            calls+=1
            if calls==2:raise OSError('simulated interruption')
            link(source,target)
        with patch.object(module.os,'link',side_effect=interrupted),self.assertRaises(OSError):module.apply(report)
        self.assertEqual(preserved,{p:p.read_bytes() for p in preserved})
        self.assertFalse(any(Path(row['target']).exists() for row in report['files']))

    def test_integrated_source_detection_uses_contents_instead_of_folder_name(self):
        self.main_package()
        self.assertEqual(module.detect_source(self.target),self.source)

    def test_integrated_multiple_readable_sources_require_a_choice(self):
        self.main_package()
        shutil.copytree(self.source,self.root/'another download')
        with self.assertRaisesRegex(ValueError,'Several Dancer downloads'):module.detect_source(self.target)

    def test_integrated_missing_source_is_a_clear_installation_error(self):
        self.main_package()
        (self.source/module.COLOURS['emerald'][0]/module.TEXTURE).unlink()
        with self.assertRaisesRegex(ValueError,'Dancer source not found'):module.detect_source(self.target)

if __name__ == '__main__':
    unittest.main()

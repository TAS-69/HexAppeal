from pathlib import Path
import importlib.util
import json
import tempfile
import unittest

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

if __name__ == '__main__':
    unittest.main()

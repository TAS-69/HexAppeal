from pathlib import Path
import copy, sys, unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from witch_parts import PRIVATE_BOOTS, clone_boot, isolated_model, partial_registry
from part_registry import read_registry


class WitchParts(unittest.TestCase):
    def fixture(self):
        return {'header': bytes(8), 'primary': [{'name': name, 'directory': '1_pc/02_phw/armor/12_foot', 'socket': '', 'flag': 1, 'asset': 'Foot_heel_1_7', 'flag2': 0, 'children': [('Foot', 1)]} for name in PRIVATE_BOOTS], 'secondary': []}

    def test_private_references_leave_the_original_registry_and_prefab_intact(self):
        registry = self.fixture()
        before = copy.deepcopy(registry)
        rows = []
        for name in PRIVATE_BOOTS:
            model = 'character/model/1_pc/2_phw/armor/12_foot/' + name.replace('_index01', '') + '.pac'
            prefab = b'prefix\0' + model.encode() + b'\0suffix'
            row, target, changed = clone_boot(registry, name, prefab, model)
            self.assertEqual(len(changed), len(prefab))
            self.assertEqual(changed, b'prefix\0' + target.encode() + b'\0suffix')
            self.assertIn('/ha_p/', target)
            self.assertEqual(row['children'], registry['primary'][0]['children'])
            rows.append(row)
        generated = read_registry(partial_registry(registry, rows))
        self.assertEqual(len(generated['primary']), 3)
        self.assertEqual(generated['secondary'], [])
        self.assertEqual(registry, before)

    def test_unknown_or_ambiguous_prefabs_fail_before_output(self):
        registry = self.fixture()
        name = next(iter(PRIVATE_BOOTS))
        model = 'character/model/1_pc/2_phw/armor/12_foot/' + name + '.pac'
        for prefab in (b'no matching reference', model.encode() * 2):
            with self.assertRaises(ValueError):
                clone_boot(registry, name, prefab, model)
        registry['primary'].append(copy.deepcopy(registry['primary'][0]))
        with self.assertRaises(ValueError):
            clone_boot(registry, name, model.encode(), model)
        with self.assertRaises(ValueError):
            partial_registry(self.fixture(), [])

    def test_other_characters_and_unrelated_boots_keep_their_paths(self):
        for path in ('0009/character/model/1_pc/2_phw/armor/12_foot/cd_phw_00_foot_00_0162.pac', '0009/other/cd_phw_00_foot_00_0160.pac'):
            self.assertEqual(isolated_model(path), path)


if __name__ == '__main__':
    unittest.main()

from pathlib import Path
import copy, struct, sys, unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from part_registry import read_registry, write_registry


class PartRegistry(unittest.TestCase):
    def fixture(self):
        return {'header': bytes(8), 'primary': [{'name': 'example_boot', 'directory': 'example/boots', 'socket': '', 'flag': 1, 'asset': 'heel', 'flag2': 0, 'children': [('Foot', 1)]}], 'secondary': [('example_alias', 'example_boot')]}

    def test_round_trip_and_private_clone_preserve_original(self):
        source = self.fixture()
        original = write_registry(source)
        decoded = read_registry(original)
        self.assertEqual(decoded, source)
        row = copy.deepcopy(decoded['primary'][0])
        row.update(name='private_boot', directory='private/boots')
        private = {'header': decoded['header'], 'primary': [row], 'secondary': []}
        self.assertEqual(read_registry(write_registry(private)), private)
        self.assertEqual(write_registry(decoded), original)

    def test_corrupt_counts_strings_and_trailing_data_are_rejected(self):
        data = write_registry(self.fixture())
        invalid = [data[:7], data[:-1], data + b'x', data[:8] + struct.pack('<I', 100001) + data[12:], data[:12] + b'\0' + data[13:]]
        for candidate in invalid:
            with self.subTest(candidate=candidate[:16]), self.assertRaises(ValueError):
                read_registry(candidate)

    def test_invalid_output_cannot_be_written(self):
        for change in ('duplicate', 'long_string', 'nul_string', 'flag', 'children'):
            data = self.fixture()
            row = data['primary'][0]
            if change == 'duplicate': data['primary'].append(copy.deepcopy(row))
            if change == 'long_string': row['name'] = 'a' * 255
            if change == 'nul_string': row['name'] = 'bad\0name'
            if change == 'flag': row['flag'] = 256
            if change == 'children': row['children'] *= 256
            with self.subTest(change=change), self.assertRaises(ValueError):
                write_registry(data)


if __name__ == '__main__':
    unittest.main()

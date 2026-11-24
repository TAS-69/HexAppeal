from pathlib import Path
import struct, sys, unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shape_follow import (active_influences, blend, blend_amount, decode_skin, encode_skin,
                          fold_to_palette, improved, quantize, smooth)


def evaluation(penetrating, deepest, reversed_triangles, near=100):
    return {'near_body_vertices': near, 'poses': {
        'p': {'penetrating_vertices': penetrating, 'deepest_mm': deepest, 'reversed_triangles': reversed_triangles}}}


def record(slots, weights, six=True, filler=0x5A):
    data = bytearray([filler] * 40)
    low = slots[0] | (slots[1] << 10) | (slots[2] << 20) | (0x80 << 24)
    high = slots[3] | (slots[4] << 10) | (slots[5] << 20)
    struct.pack_into('<II', data, 20, low, high)
    data[28:36] = bytes(weights)
    data[39] = 0xC0 | (63 if six else 12)
    return data


class ShapeFollow(unittest.TestCase):
    def test_quantize_keeps_strongest_and_sums_to_255(self):
        pairs = quantize({1: .5, 2: .3, 3: .1, 4: .05, 5: .03, 6: .01, 7: .005, 8: .005}, limit=6)
        self.assertEqual(len(pairs), 6)
        self.assertEqual(sum(w for _, w in pairs), 255)
        self.assertEqual(pairs[0][0], 1)
        self.assertNotIn(7, dict(pairs))

    def test_quantize_rejects_empty_weights(self):
        with self.assertRaises(ValueError):
            quantize({1: 0.0}, 4)

    def test_fold_moves_weight_to_nearest_palette_ancestor(self):
        parent = [-1, 0, 1, 2, -1]
        folded = fold_to_palette({3: .4, 1: .5, 4: .1}, {1}, parent)
        self.assertEqual(folded, {1: .9})

    def test_blend_amount_runs_from_full_to_none(self):
        self.assertEqual(blend_amount(0.005, 0.01, 0.06), 1.0)
        self.assertEqual(blend_amount(-0.005, 0.01, 0.06), 1.0)
        self.assertAlmostEqual(blend_amount(0.035, 0.01, 0.06), 0.5)
        self.assertEqual(blend_amount(0.07, 0.01, 0.06), 0.0)

    def test_blend_mixes_garment_and_body(self):
        self.assertEqual(blend({1: 1.0}, {2: 1.0}, 0.25), {1: .75, 2: .25})

    def test_influence_count_comes_from_byte_39(self):
        self.assertEqual(active_influences(record([0] * 6, [255] + [0] * 7, six=True)), 6)
        self.assertEqual(active_influences(record([0] * 6, [255] + [0] * 7, six=False)), 4)

    def test_six_influence_round_trip_keeps_other_bytes(self):
        data = record([1, 2, 3, 4, 5, 6], [100, 50, 40, 30, 20, 15, 77, 88])
        self.assertEqual(decode_skin(data), ([1, 2, 3, 4, 5, 6], [100, 50, 40, 30, 20, 15]))
        before = bytes(data)
        encode_skin(data, [900, 1, 2, 3, 4, 5], [200, 20, 10, 10, 10, 5])
        self.assertEqual(decode_skin(data), ([900, 1, 2, 3, 4, 5], [200, 20, 10, 10, 10, 5]))
        self.assertEqual(bytes(data[:20]), before[:20], 'position, texture coordinates and normal are kept')
        self.assertEqual(bytes(data[34:]), before[34:], 'unread weights and byte 39 are kept')
        self.assertEqual(data[23] & 0xC0, 0x80, 'spare high bits are kept')

    def test_four_influence_records_keep_unread_slots_and_weights(self):
        data = record([1, 2, 3, 4, 700, 800], [120, 100, 30, 5, 66, 77, 88, 99], six=False)
        before = bytes(data)
        encode_skin(data, [9, 8, 7, 6], [128, 64, 32, 31])
        self.assertEqual(decode_skin(data), ([9, 8, 7, 6], [128, 64, 32, 31]))
        low, high = struct.unpack_from('<II', data, 24 - 4)
        self.assertEqual(((high >> 10) & 0x3FF, (high >> 20) & 0x3FF), (700, 800))
        self.assertEqual(bytes(data[32:]), before[32:])
        with self.assertRaises(ValueError):
            encode_skin(data, [1, 2, 3, 4, 5, 6], [100, 100, 55, 0, 0, 0])

    def test_smoothing_moves_only_movable_vertices(self):
        weights = [{1: 1.0}, {2: 1.0}, {1: 1.0}]
        neighbours = [{1}, {0, 2}, {1}]
        result = smooth(weights, neighbours, [False, True, False], 1)
        self.assertEqual(result[0], {1: 1.0})
        self.assertEqual(result[2], {1: 1.0})
        self.assertAlmostEqual(result[1][2], 0.5)
        self.assertAlmostEqual(result[1][1], 0.5)

    def test_gate_keeps_only_garments_that_clip_less(self):
        creases = {'p': 20}
        self.assertTrue(improved(evaluation(50, 30.0, 5), evaluation(2, 4.0, 30), creases)[0])
        self.assertFalse(improved(evaluation(5, 3.0, 5), evaluation(5, 2.0, 5), creases)[0], 'no fewer penetrations')
        self.assertFalse(improved(evaluation(5, 3.0, 5), evaluation(1, 6.0, 5), creases)[0], 'deeper worst case')
        self.assertFalse(improved(evaluation(50, 30.0, 5), evaluation(2, 4.0, 40), creases)[0], 'too many new creases')
        self.assertFalse(improved(evaluation(0, 0.0, 0, near=0), evaluation(0, 0.0, 0, near=0), creases)[0])

    def test_out_of_range_values_are_refused(self):
        data = record([0] * 6, [255, 0, 0, 0, 0, 0, 0, 0])
        with self.assertRaises(ValueError):
            encode_skin(data, [1024] + [0] * 5, [255] + [0] * 5)


if __name__ == '__main__':
    unittest.main()

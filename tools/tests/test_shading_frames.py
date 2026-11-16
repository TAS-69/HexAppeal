import sys
import struct
import unittest
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from shading_frames import decode_pac_tangent, field_jacobians, transport_frames


class ShadingFramesTests(unittest.TestCase):
    def setUp(self):
        self.points = np.array([[.1, .2, .3], [-.2, .7, .05], [.4, -.1, .6]])
        self.normals = np.tile(np.array([1., 2., 3.])/np.sqrt(14), (3, 1))
        self.tangents = np.tile(np.array([2., -1., 0.])/np.sqrt(5), (3, 1))

    def test_translation_and_uniform_scale_keep_authored_frames(self):
        for scale in [1., .6, 1.4]:
            with self.subTest(scale=scale):
                n, t, changed, _ = transport_frames(self.points, self.normals, self.tangents, lambda p: p*scale + [.2, -.4, .1])
                np.testing.assert_allclose(n, self.normals, atol=1e-14)
                np.testing.assert_allclose(t, self.tangents, atol=1e-14)
                self.assertFalse(changed.any())

    def test_rotation_carries_both_directions(self):
        angle = .41
        rotation = np.array([[np.cos(angle), 0, np.sin(angle)], [0, 1, 0], [-np.sin(angle), 0, np.cos(angle)]])
        n, t, changed, _ = transport_frames(self.points, self.normals, self.tangents, lambda p: p@rotation.T)
        np.testing.assert_allclose(n, self.normals@rotation.T, atol=1e-10)
        np.testing.assert_allclose(t, self.tangents@rotation.T, atol=1e-10)
        self.assertTrue(changed.all())

    def test_shear_uses_inverse_transpose_for_normals(self):
        matrix = np.array([[1.3, .2, .1], [0, .8, .15], [.1, 0, 1.1]])
        n, t, _, _ = transport_frames(self.points, self.normals, self.tangents, lambda p: p@matrix.T)
        expected = self.normals@np.linalg.inv(matrix)
        expected /= np.linalg.norm(expected, axis=1, keepdims=True)
        np.testing.assert_allclose(n, expected, atol=1e-10)
        np.testing.assert_allclose(np.sum(n*t, axis=1), 0, atol=1e-12)
        np.testing.assert_allclose(np.sum(n*(self.tangents@matrix.T), axis=1), 0, atol=1e-10)

    def test_nonlinear_field_matches_analytic_derivative(self):
        def field(p):
            out = p.copy(); out[:, 0] += .2*p[:, 0]*p[:, 1]; out[:, 1] += .1*p[:, 2]**2
            return out
        expected = np.tile(np.eye(3), (3, 1, 1))
        expected[:, 0, 0] += .2*self.points[:, 1]; expected[:, 0, 1] = .2*self.points[:, 0]; expected[:, 1, 2] = .2*self.points[:, 2]
        np.testing.assert_allclose(field_jacobians(self.points, field), expected, atol=1e-10)

    def test_reflection_singular_and_nonfinite_fields_are_rejected(self):
        fields = [lambda p: p*[-1, 1, 1], lambda p: p*[1, 0, 1], lambda p: p*[.001, 1000, 1], lambda p: p*np.nan, lambda p: p[:, :2]]
        for field in fields:
            with self.subTest(field=field), self.assertRaises(ValueError):
                transport_frames(self.points, self.normals, self.tangents, field)

    def test_invalid_frames_are_rejected(self):
        for normals, tangents in [(self.normals*0, self.tangents), (self.normals, self.normals), (self.normals[:1], self.tangents)]:
            with self.subTest(normals=normals), self.assertRaises(ValueError):
                transport_frames(self.points, normals, tangents, lambda p: p)

    def test_packed_tangent_sign_and_handedness(self):
        for z_sign in [-1, 1]:
            for handedness in [-1, 1]:
                record = bytearray(40)
                struct.pack_into('<h', record, 6, 16384*z_sign)
                struct.pack_into('<I', record, 16, 512 | (0x80000000 if handedness < 0 else 0))
                tangent, sign = decode_pac_tangent(record)
                self.assertEqual(sign, handedness)
                self.assertLess(abs(tangent[0]), .001); self.assertLess(abs(tangent[1]), .001)
                self.assertGreater(tangent[2]*z_sign, .999999)
        with self.assertRaises(ValueError): decode_pac_tangent(bytes(19))

    def test_empty_frames(self):
        empty = np.empty((0, 3))
        n, t, changed, _ = transport_frames(empty, empty, empty, lambda p: p)
        self.assertEqual(n.shape, (0, 3)); self.assertEqual(t.shape, (0, 3)); self.assertFalse(changed.any())

    def test_invalid_differentiation_distance(self):
        for epsilon in [0, -1, np.nan, np.inf]:
            with self.subTest(epsilon=epsilon), self.assertRaises(ValueError):
                field_jacobians(self.points, lambda p: p, epsilon)


if __name__ == '__main__':
    unittest.main()

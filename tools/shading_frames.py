"""Carry authored shading frames through a positional deformation field."""
import struct
import numpy as np


def unit_rows(values, label):
    values = np.asarray(values, dtype=float)
    if values.ndim != 2 or values.shape[1] != 3 or not np.isfinite(values).all():
        raise ValueError(f'{label} must be finite three-component vectors.')
    lengths = np.linalg.norm(values, axis=1, keepdims=True)
    if np.any(lengths < 1e-10):
        raise ValueError(f'{label} contains a degenerate vector.')
    return values / lengths


def decode_pac_tangent(record):
    if len(record) < 20:
        raise ValueError('PAC tangent requires a complete shading record.')
    signed = struct.unpack_from('<h', record, 6)[0]
    packed = struct.unpack_from('<I', record, 16)[0]
    x = abs(signed) / 16383.75 - 1.0
    y = (packed & 1023) / 511.5 - 1.0
    z = np.sqrt(max(0.0, 1.0 - x*x - y*y)) * (-1.0 if signed < 0 else 1.0)
    tangent = unit_rows([[x, y, z]], 'Packed tangent')[0]
    return tangent, -1.0 if packed & 0x80000000 else 1.0


def field_jacobians(points, field, epsilon=0.0005):
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
        raise ValueError('Field points must be finite three-component positions.')
    if not np.isfinite(epsilon) or epsilon <= 0:
        raise ValueError('The differentiation distance must be positive and finite.')
    matrices = np.empty((len(points), 3, 3))
    for axis in range(3):
        delta = np.zeros(3); delta[axis] = epsilon
        plus, minus = [np.asarray(field(points + offset), dtype=float) for offset in [delta, -delta]]
        if plus.shape != points.shape or minus.shape != points.shape or not np.isfinite(plus).all() or not np.isfinite(minus).all():
            raise ValueError('The deformation field returned invalid positions.')
        matrices[:, :, axis] = (plus - minus) / (2*epsilon)
    return matrices


def transport_frames(points, normals, tangents, field, epsilon=0.0005):
    normals = unit_rows(normals, 'Authored normals')
    tangents = unit_rows(tangents, 'Authored tangents')
    matrices = field_jacobians(points, field, epsilon)
    if len(normals) != len(matrices) or len(tangents) != len(matrices):
        raise ValueError('Shading frames and field points have different counts.')
    if not len(matrices):
        return normals, tangents, np.zeros(0, dtype=bool), {'min_determinant': 1.0, 'max_determinant': 1.0, 'max_condition': 1.0}
    determinants = np.linalg.det(matrices)
    conditions = np.linalg.cond(matrices)
    if np.any(determinants <= 1e-5) or not np.isfinite(conditions).all() or np.any(conditions > 100):
        raise ValueError('Shading transport requires an invertible orientation-preserving field.')
    if np.any(np.abs(np.sum(normals*tangents, axis=1)) > .25):
        raise ValueError('Authored normal/tangent frames are incompatible.')
    transported_normals = unit_rows(np.linalg.solve(matrices.transpose(0, 2, 1), normals[:, :, None])[:, :, 0], 'Transported normals')
    transported_tangents = np.einsum('nij,nj->ni', matrices, tangents)
    transported_tangents -= np.sum(transported_tangents*transported_normals, axis=1)[:, None]*transported_normals
    transported_tangents = unit_rows(transported_tangents, 'Transported tangents')
    scale = np.trace(matrices, axis1=1, axis2=2) / 3
    isotropic = np.max(np.abs(matrices - scale[:, None, None]*np.eye(3)), axis=(1, 2)) < 1e-8
    transported_normals[isotropic] = normals[isotropic]
    transported_tangents[isotropic] = tangents[isotropic]
    changed = (~isotropic) & ((np.linalg.norm(transported_normals-normals, axis=1) > 1e-7) | (np.linalg.norm(transported_tangents-tangents, axis=1) > 1e-7))
    return transported_normals, transported_tangents, changed, {
        'min_determinant': float(determinants.min()), 'max_determinant': float(determinants.max()), 'max_condition': float(conditions.max())}

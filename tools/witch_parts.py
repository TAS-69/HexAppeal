from pathlib import PurePosixPath
import copy

from part_registry import read_registry, write_registry


PRIVATE_BOOTS = {
    'cd_phw_00_foot_00_0160': 'ha_phw_00_foot_00_0160',
    'cd_phw_00_foot_00_0161_index01': 'ha_phw_00_foot_00_0161_index01',
    'cd_phw_00_foot_00_0163': 'ha_phw_00_foot_00_0163',
}


def isolated_model(path):
    value = PurePosixPath(path)
    if value.name not in {'cd_phw_00_foot_00_0160.pac', 'cd_phw_00_foot_00_0161.pac', 'cd_phw_00_foot_00_0163.pac'}:
        return value.as_posix()
    expected = '0009/character/model/1_pc/2_phw/armor/12_foot/' + value.name
    if value.as_posix() != expected:
        return value.as_posix()
    return expected.replace('/1_pc/', '/ha_p/', 1).replace('/cd_phw_', '/ha_phw_', 1)


def clone_boot(registry, name, prefab, model):
    if name not in PRIVATE_BOOTS:
        raise ValueError('Only the three shared witch boots are isolated')
    matches = [row for row in registry['primary'] if row['name'] == name]
    if len(matches) != 1 or any(row['name'] == PRIVATE_BOOTS[name] for row in registry['primary']):
        raise ValueError('Missing, ambiguous or already registered witch boot')
    row = copy.deepcopy(matches[0])
    if row['directory'] != '1_pc/02_phw/armor/12_foot':
        raise ValueError('The source boot registration changed')
    row.update(name=PRIVATE_BOOTS[name], directory='ha_p/02_phw/armor/12_foot')
    model_path = '0009/' + model
    private_model = isolated_model(model_path)
    if private_model == model_path:
        raise ValueError('The source boot model path changed')
    private_model = private_model.removeprefix('0009/')
    before, after = model.encode('utf-8'), private_model.encode('utf-8')
    if len(before) != len(after) or prefab.count(before) != 1:
        raise ValueError('The compiled prefab does not contain exactly one supported model reference')
    return row, private_model, prefab.replace(before, after)


def partial_registry(registry, rows):
    if {row['name'] for row in rows} != set(PRIVATE_BOOTS.values()):
        raise ValueError('The private boot registry must contain exactly three parts')
    value = {'header': registry['header'], 'primary': rows, 'secondary': []}
    data = write_registry(value)
    if read_registry(data) != value:
        raise ValueError('The generated part registry did not round trip')
    return data

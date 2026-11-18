import struct


def read_registry(data):
    at = 0

    def take(count):
        nonlocal at
        if count < 0 or at + count > len(data):
            raise ValueError('Truncated part registry')
        value = data[at:at + count]
        at += count
        return value

    def byte():
        return take(1)[0]

    def count():
        value = struct.unpack('<I', take(4))[0]
        if value > 100000 or value > len(data) - at:
            raise ValueError('Invalid part registry count')
        return value

    def text():
        size = byte()
        value = take(size)
        if not size or value[-1] or b'\0' in value[:-1]:
            raise ValueError('Invalid part registry string')
        return value[:-1].decode('utf-8')

    result = {'header': take(8), 'primary': [], 'secondary': []}
    for _ in range(count()):
        row = {'name': text(), 'directory': text(), 'socket': text(), 'flag': byte(), 'asset': text(), 'flag2': byte()}
        row['children'] = [(text(), byte()) for _ in range(byte())]
        result['primary'].append(row)
    for _ in range(count()):
        result['secondary'].append((text(), text()))
    if at != len(data):
        raise ValueError('Trailing part registry data')
    return result


def write_registry(registry):
    header = registry['header']
    if len(header) != 8:
        raise ValueError('Invalid part registry header')

    def byte(value):
        if not isinstance(value, int) or not 0 <= value <= 255:
            raise ValueError('Invalid part registry byte')
        return bytes([value])

    def text(value):
        raw = value.encode('utf-8')
        if b'\0' in raw or len(raw) > 254:
            raise ValueError('Invalid part registry string')
        return byte(len(raw) + 1) + raw + b'\0'

    primary, secondary = registry['primary'], registry['secondary']
    if max(len(primary), len(secondary)) > 100000:
        raise ValueError('Invalid part registry count')
    out = bytearray(header + struct.pack('<I', len(primary)))
    names = set()
    for row in primary:
        if row['name'] in names:
            raise ValueError('Duplicate part registry name')
        names.add(row['name'])
        out += text(row['name']) + text(row['directory']) + text(row['socket'])
        out += byte(row['flag']) + text(row['asset']) + byte(row['flag2']) + byte(len(row['children']))
        for name, flag in row['children']:
            out += text(name) + byte(flag)
    out += struct.pack('<I', len(secondary))
    for name, alias in secondary:
        out += text(name) + text(alias)
    return bytes(out)

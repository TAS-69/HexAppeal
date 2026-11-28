"""Generate signatures for the 2.06 addresses that are not in the required layout.

Run against the checked Steam 2.06 CrimsonDesert.exe. Writes signatures206.json;
emit206.py turns it into runtime/src/game_signatures_206.inc.

Kinds:
  function  pattern grown from the function entry (or, if the entry is not
            unique, from a unique call to it)
  site      return address after a call; pattern grown from a few instructions
            before it; the runtime checks the call targets 'calls'
  data      RIP-relative reference to a global, read back from its disp32
  rtti      vtable of a class, found through its RTTI name
  slot      a function read from a vtable slot of an RTTI class

Relocatable fields (call/jump targets, RIP-relative displacements, absolute
addresses) are wildcarded, so the patterns also match another link of the same
code, such as a different store's executable.
"""
import json, mmap, re, struct, subprocess, sys
from pathlib import Path

EXE = sys.argv[1] if len(sys.argv) > 1 else r'C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert\bin64\CrimsonDesert.exe'
SOURCE = dict(stamp=0x6b07e5e9, size=0x13d05000, checksum=0x1380bc2b)

# name, checked Steam 2.06 RVA, kind, extra
FIELDS = [
    ('poseUpload',    0x324fd80, 'function', None),
    ('menuChoose',    0x1325f40, 'function', None),
    ('menuCommit',    0x1326390, 'function', None),
    ('menuDisplay',   0x131dd80, 'function', None),
    ('menuRefresh',   0x131da90, 'function', None),
    ('menuResolve',   0x7819d0,  'function', None),
    ('menuRelease',   0x16e3b60, 'function', None),
    ('widgetFind',    0x3a75cf0, 'function', None),
    ('widgetResolve', 0x738010,  'function', None),
    ('widgetSet',     0x777a70,  'function', None),
    ('chooseCaller',  0x131b192, 'site', 'menuChoose'),
    ('commitDisplay', 0x1326eb1, 'site', 'menuDisplay'),
    ('ia_alt0',       0xb3e5a5,  'site', 'ia_build'),
    ('ia_alt1',       0xb3e7bc,  'site', 'ia_build'),
    ('ia_alt2',       0xb3f655,  'site', 'ia_build'),
    ('ia_alt3',       0xc5de0d,  'site', 'ia_build'),
    ('closeEvent',    0x734fdd4, 'data', None),
    ('cursorClass',   0x5c7f9d8, 'rtti', '.?AVUIGamePlayControlCommon_RadialCursor@uiCommonScript@pa@@'),
    ('menuClose',     0x131bac0, 'slot', ('.?AVUIGamePlayControlCommon_Interaction@uiCommonScript@pa@@', 0x510)),
]
MINIMUM = 16  # solid bytes; a little more than gen.py so another link stays unique

f = open(EXE, 'rb'); d = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
pe = struct.unpack_from('<I', d, 0x3c)[0]; nsec = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]; opt = pe + 24
BASE = struct.unpack_from('<Q', d, opt + 24)[0]; SIZE = struct.unpack_from('<I', d, opt + 56)[0]
STAMP = struct.unpack_from('<I', d, pe + 8)[0]; CHECKSUM = struct.unpack_from('<I', d, opt + 64)[0]
assert (STAMP, SIZE, CHECKSUM) == (SOURCE['stamp'], SOURCE['size'], SOURCE['checksum']), 'not the checked Steam 2.06 executable'
secs = []
for i in range(nsec):
    o = opt + osz + 40 * i
    vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8); ch = struct.unpack_from('<I', d, o + 36)[0]
    secs.append(dict(va=va, vs=vs, ra=ra, rs=rs, x=bool(ch & 0x20000000), w=bool(ch & 0x80000000)))
code = next(s for s in secs if s['x'] and not s['w'])
CODE = bytes(d[code['ra']:code['ra'] + code['rs']])

def rva_bytes(rva, n):
    for s in secs:
        if s['va'] <= rva < s['va'] + s['rs']:
            o = s['ra'] + rva - s['va']; return bytes(d[o:o + n])
    return None

ENV_CMD = Path(__file__).resolve().parents[2] / 'build' / 'msvc-env.cmd'
ENV = {}
for line in subprocess.run(['cmd', '/c', ENV_CMD], capture_output=True, text=True).stdout.splitlines():
    if '=' in line: k, v = line.split('=', 1); ENV[k] = v
DUMPBIN = next(str(Path(p) / 'dumpbin.exe') for p in ENV.get('Path', ENV.get('PATH', '')).split(';') if p and (Path(p) / 'dumpbin.exe').exists())

def disasm(lo, hi):
    out = subprocess.run([DUMPBIN, '/nologo', '/disasm:nobytes', f'/range:0x{BASE+lo:X},0x{BASE+hi:X}', EXE],
                         capture_output=True, text=True, env=ENV).stdout
    rows = []
    for line in out.splitlines():
        m = re.match(r'\s+([0-9A-F]{16}): (.*)$', line)
        if m: rows.append((int(m.group(1), 16) - BASE, m.group(2)))
    return [(r, rows[i + 1][0] - r, t) for i, (r, t) in enumerate(rows[:-1])]

def disasm_through(target, before, after):
    for back in (before, before + 0x40, before + 0x100, before - 7, before - 3, before + 0x13, 0):
        ins = disasm(max(code['va'], target - back), target + after)
        if any(r == target for r, _, _ in ins): return ins
    return disasm(target, target + after)

def mask(rva, length, text):
    raw = list(rva_bytes(rva, length)); out = list(raw)
    for target in re.findall(r'\b(000000014[0-9A-F]{7})h?\b', text):
        t = int(target, 16); rel = (t - BASE) - (rva + length)
        for value, size in ((struct.pack('<i', rel) if -2**31 <= rel < 2**31 else None, 4), (struct.pack('<Q', t), 8)):
            if value is None: continue
            k = bytes(raw).find(value)
            if k >= 0:
                for j in range(k, k + size): out[j] = None
    # Short and near branches inside the function keep their bytes; nothing else is image-relative.
    return out

def count(pattern, limit=2):
    runs, start = [], None
    for i, b in enumerate(pattern + [None]):
        if b is not None and start is None: start = i
        if b is None and start is not None: runs.append((start, i)); start = None
    if not runs: return 99
    a, z = max(runs, key=lambda r: r[1] - r[0]); anchor = bytes(pattern[a:z])
    found, pos = 0, CODE.find(anchor)
    while pos >= 0:
        s = pos - a
        if s >= 0 and s + len(pattern) <= len(CODE) and all(p is None or CODE[s + i] == p for i, p in enumerate(pattern)):
            found += 1
            if found >= limit: return found
        pos = CODE.find(anchor, pos + 1)
    return found

def solid(p): return sum(b is not None for b in p)

def grow(instructions, first, anchor_rva, maximum=120):
    pattern = []; start = instructions[first][0]
    for rva, length, text in instructions[first:]:
        pattern += mask(rva, length, text)
        if len(pattern) > maximum: return None
        if start + len(pattern) <= anchor_rva: continue
        if solid(pattern) >= MINIMUM and count(pattern) == 1:
            return pattern, anchor_rva - start
    return None

def fmt(p): return ' '.join('??' if b is None else f'{b:02X}' for b in p)

def function_signature(rva):
    ins = disasm(rva, rva + 0xc0)
    assert ins and ins[0][0] == rva
    return grow(ins, 0, rva)

def site_signature(rva):
    ins = disasm_through(rva - 5, 0x30, 0xa0)
    at = next(i for i, x in enumerate(ins) if x[0] == rva - 5)
    assert rva_bytes(rva - 5, 1) == b'\xe8'
    best = None
    for back in range(0, 7):
        if at - back < 0: break
        result = grow(ins, at - back, rva)
        if result and (best is None or len(result[0]) < len(best[0])): best = result
    return best

def references(target):
    """(disp32 rva, bytes after disp32) for RIP-relative references to target."""
    import numpy as np
    hits = []
    for a in range(4):
        n = (len(CODE) - a) // 4
        disp = np.frombuffer(CODE, dtype='<i4', count=n, offset=a).astype(np.int64)
        pos = code['va'] + a + 4 * np.arange(n, dtype=np.int64)
        for tail in (0, 1, 4):
            for h in np.nonzero(pos + 4 + tail + disp == target)[0].tolist(): hits.append((int(pos[h]), tail))
    return sorted(hits)

def reference_signature(target, calls_only=False):
    candidates = []
    for disp_rva, tail in references(target)[:40]:
        for back in (0x28, 0x40, 0x100, 0x21, 0x13):
            ins = disasm(disp_rva - back, disp_rva + 0x50)
            hit = [i for i, (r, l, t) in enumerate(ins) if r < disp_rva < r + l and r + l == disp_rva + 4 + tail]
            if hit: break
        if not hit: continue
        at = hit[0]; text = ins[at][2]
        if calls_only and not text.lstrip().startswith('call'): continue
        for back in range(0, 5):
            if at - back < 0: break
            result = grow(ins, at - back, ins[at][0])
            if result:
                pattern, ref_offset = result
                candidates.append(dict(pattern=pattern, disp=disp_rva - ins[at - back][0], end=ref_offset + ins[at][1], text=text))
                break
    candidates.sort(key=lambda c: len(c['pattern']))
    return candidates[0] if candidates else None

def rtti_vtable(name):
    """Mirrors the runtime: type descriptor -> complete object locator -> vtable."""
    raw = name.encode() + b'\0'
    hits = []
    for s in secs:
        if s['x']: continue
        blob = d[s['ra']:s['ra'] + s['rs']]; p = blob.find(raw)
        while p >= 0: hits.append(s['va'] + p); p = blob.find(raw, p + 1)
    assert len(hits) == 1, (name, hits)
    descriptor = hits[0] - 16
    locators = []
    for s in secs:
        if s['x'] or s['w']: continue
        blob = d[s['ra']:s['ra'] + s['rs']]
        for i in range(0, len(blob) - 24, 4):
            if struct.unpack_from('<I', blob, i + 12)[0] == descriptor and struct.unpack_from('<I', blob, i)[0] == 1 \
               and struct.unpack_from('<I', blob, i + 20)[0] == s['va'] + i:
                locators.append(s['va'] + i)
    assert len(locators) == 1, (name, locators)
    pointer = struct.pack('<Q', BASE + locators[0]); slots = []
    for s in secs:
        if s['x'] or s['w']: continue
        blob = d[s['ra']:s['ra'] + s['rs']]; p = blob.find(pointer)
        while p >= 0:
            if p % 8 == 0: slots.append(s['va'] + p)
            p = blob.find(pointer, p + 1)
    assert len(slots) == 1, (name, slots)
    return slots[0] + 8

results = []
for name, rva, kind, extra in FIELDS:
    row = dict(name=name, rva=rva, kind=kind)
    if kind == 'function':
        res = function_signature(rva)
        if res:
            row.update(method='code', pattern=fmt(res[0]), offset=res[1])
        else:
            via = reference_signature(rva, calls_only=True)
            assert via, f'no signature for {name}'
            row.update(method='reference', pattern=fmt(via['pattern']), disp=via['disp'], end=via['end'], via=via['text'])
    elif kind == 'site':
        res = site_signature(rva)
        assert res, f'no signature for {name}'
        row.update(method='code', pattern=fmt(res[0]), offset=res[1], calls=extra)
    elif kind == 'data':
        via = reference_signature(rva)
        assert via, f'no signature for {name}'
        row.update(method='reference', pattern=fmt(via['pattern']), disp=via['disp'], end=via['end'], via=via['text'])
    elif kind == 'rtti':
        assert rtti_vtable(extra) == rva, name
        row.update(method='rtti', cls=extra)
    elif kind == 'slot':
        cls, slot = extra
        vtable = rtti_vtable(cls)
        assert struct.unpack('<Q', rva_bytes(vtable + slot, 8))[0] - BASE == rva, name
        row.update(method='slot', cls=cls, slot=slot)
    print(name, row['method'], row.get('pattern', row.get('cls')), file=sys.stderr)
    results.append(row)

out = dict(exe=dict(stamp=STAMP, size=SIZE, checksum=CHECKSUM), fields=results)
Path(__file__).with_name('signatures206.json').write_text(json.dumps(out, indent=1))
print('wrote', len(results), 'fields')

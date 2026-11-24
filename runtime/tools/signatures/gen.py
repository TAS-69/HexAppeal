"""Generate wildcard signatures for Hex Appeal's game layout from CrimsonDesert.exe.

Code locations: pattern grown from the location (or from a few instructions
before it) until unique in the executable, non-writable section.
Data (vtables, globals): pattern around an instruction that references the
address RIP-relatively; the target is read back from its disp32.
Displacements and absolute addresses are wildcarded so a patch that moves code
or data keeps the pattern valid. Output: signatures.json.
"""
import concurrent.futures, json, mmap, re, struct, subprocess, sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from layout1 import FIELDS

EXE = r'C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert\bin64\CrimsonDesert.exe'
VC = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
CODE_KIND = {'frameCalls0': 'call', 'frameCalls1': 'call', 'hudCall': 'call', 'hudSubmit': 'site',
             'ia_focus0': 'site', 'ia_focus1': 'site', 'ia_focus2': 'site'}
FUNCTIONS = {'frameDispatch', 'visibilityObserver', 'visibilitySet', 'visibilityGet', 'internString',
             'rebuildAppearance', 'mergeAppearance', 'copyPart', 'releaseWrapper', 'destroyRecord',
             'copyRecord', 'applyAppearance',
             'ia_build', 'ia_evaluate', 'ia_select', 'ia_reserve', 'ia_copy', 'ia_makeLabel', 'ia_copyLabel'}

f = open(EXE, 'rb'); d = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
pe = struct.unpack_from('<I', d, 0x3c)[0]; nsec = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]; opt = pe + 24
BASE = struct.unpack_from('<Q', d, opt + 24)[0]; SIZE = struct.unpack_from('<I', d, opt + 56)[0]
secs = []
for i in range(nsec):
    o = opt + osz + 40 * i; nm = d[o:o + 8].rstrip(b'\0').decode()
    vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8); ch = struct.unpack_from('<I', d, o + 36)[0]
    secs.append(dict(name=nm, va=va, vs=vs, ra=ra, rs=rs, x=bool(ch & 0x20000000), w=bool(ch & 0x80000000)))
code = next(s for s in secs if s['x'] and not s['w'])
CODE = bytes(d[code['ra']:code['ra'] + code['rs']])
def rva_bytes(rva, n):
    for s in secs:
        if s['va'] <= rva < s['va'] + s['rs']:
            o = s['ra'] + rva - s['va']; return bytes(d[o:o + n])
    return None

ENV_CMD = Path(__file__).resolve().parents[2] / "build" / "msvc-env.cmd"
ENV = {}
for line in subprocess.run(['cmd', '/c', ENV_CMD], capture_output=True, text=True).stdout.splitlines():
    if '=' in line: k, v = line.split('=', 1); ENV[k] = v
DUMPBIN = next(str(Path(p) / 'dumpbin.exe') for p in ENV.get('Path', ENV.get('PATH', '')).split(';') if p and (Path(p) / 'dumpbin.exe').exists())

def disasm(lo, hi):
    """[(rva, length, text)] for instructions in [lo, hi)."""
    out = subprocess.run([DUMPBIN, '/nologo', '/disasm:nobytes', f'/range:0x{BASE+lo:X},0x{BASE+hi:X}', EXE],
                         capture_output=True, text=True, env=ENV).stdout
    rows = []
    for line in out.splitlines():
        m = re.match(r'\s+([0-9A-F]{16}): (.*)$', line)
        if m: rows.append((int(m.group(1), 16) - BASE, m.group(2)))
    return [(r, rows[i + 1][0] - r, t) for i, (r, t) in enumerate(rows[:-1])]

def disasm_through(target, before, after):
    """Disassembly that starts before 'target' and decodes an instruction exactly at it."""
    for back in (before, before + 0x40, before + 0x100, before - 7, before - 3, before + 0x13, 0):
        lo = max(code['va'], target - back)
        ins = disasm(lo, target + after)
        if any(r == target for r, _, _ in ins): return ins
    return disasm(target, target + after)

def mask(rva, length, text):
    """Bytes of one instruction with relocatable fields set to None."""
    raw = list(rva_bytes(rva, length)); out = list(raw)
    for target in re.findall(r'\b(000000014[0-9A-F]{7})h?\b', text):
        t = int(target, 16)
        rel = (t - BASE) - (rva + length)
        for value, size in ((struct.pack('<i', rel) if -2**31 <= rel < 2**31 else None, 4), (struct.pack('<Q', t), 8)):
            if value is None: continue
            k = bytes(raw).find(value)
            if k >= 0:
                for j in range(k, k + size): out[j] = None
    return out

def count(pattern, limit=2):
    """Matches of a wildcard pattern in the code section (stops at limit)."""
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

def grow(instructions, first, anchor_rva, minimum=12, maximum=110):
    """Grow a pattern from instructions[first:] until unique; returns (pattern, anchor offset)."""
    pattern = []
    start = instructions[first][0]
    for rva, length, text in instructions[first:]:
        pattern += mask(rva, length, text)
        if len(pattern) > maximum: return None
        if start + len(pattern) <= anchor_rva: continue  # the anchor instruction must be inside
        if solid(pattern) >= minimum and count(pattern) == 1:
            return pattern, anchor_rva - instructions[first][0]
    return None

def code_signature(name, rva):
    ins = disasm_through(rva, 0x30, 0xa0) if name in CODE_KIND else disasm(rva, rva + 0xa0)
    at = next(i for i, x in enumerate(ins) if x[0] == rva)
    if name in CODE_KIND and CODE_KIND[name] == 'call': assert rva_bytes(rva, 1) == b'\xe8', name
    best = None
    for back in range(0, (6 if name in CODE_KIND else 0) + 1):
        if at - back < 0: break
        result = grow(ins, at - back, rva)
        if result and (best is None or len(result[0]) < len(best[0])): best = result
    return best

# Data: every RIP-relative reference in code, by target.
DATA = {n: r for n, r in FIELDS if n not in FUNCTIONS and n not in CODE_KIND}
# Functions too: one whose own entry is not unique is found through a call to it.
targets = np.array(sorted(set(DATA.values()) | {r for n, r in FIELDS if n in FUNCTIONS}), dtype=np.int64)
refs = {r: [] for r in targets.tolist()}
arr = np.frombuffer(CODE, dtype=np.uint8)
for a in range(4):
    n = (len(CODE) - a) // 4
    disp = np.frombuffer(CODE, dtype='<i4', count=n, offset=a).astype(np.int64)
    pos = code['va'] + a + 4 * np.arange(n, dtype=np.int64)       # rva of disp32
    for tail in (0, 1, 4):                                          # bytes after disp32 in the instruction
        t = pos + 4 + tail + disp
        hit = np.nonzero(np.isin(t, targets))[0]
        for h in hit.tolist(): refs[int(t[h])].append((int(pos[h]), tail))

RTTI = {'actor': '.?AVClientNormalInGameActor@pa@@', 'control': '.?AVClientCharacterControlActorComponent@pa@@',
        'controller': '.?AVCharacterCustomizationController@pa@@', 'actorManager': '.?AVClientActorManager@pa@@',
        'actorContainer': '.?AVClientActorContainer@pa@@', 'playerActor': '.?AVClientChildOnlyInGameActor@pa@@'}
OPTIONAL = {'appearance'}
PREFER_CALL = {'ia_reserve'}

def data_signature(name, target):
    if name in RTTI: return None
    candidates = []
    for disp_rva, tail in refs[target][:40]:
        ins = disasm(disp_rva - 0x28, disp_rva + 0x50)
        hit = [i for i, (r, l, t) in enumerate(ins) if r < disp_rva < r + l and r + l == disp_rva + 4 + tail]
        if not hit:  # decoding started mid-instruction; retry from other offsets
            for back in (0x40, 0x100, 0x21, 0x13):
                ins = disasm(disp_rva - back, disp_rva + 0x50)
                hit = [i for i, (r, l, t) in enumerate(ins) if r < disp_rva < r + l and r + l == disp_rva + 4 + tail]
                if hit: break
        if not hit: continue
        at = hit[0]; text = ins[at][2]
        for back in range(0, 5):
            if at - back < 0: break
            result = grow(ins, at - back, ins[at][0])
            if result:
                pattern, ref_offset = result
                instr_end = ref_offset + ins[at][1]; disp_offset = disp_rva - ins[at - back][0]
                candidates.append(dict(pattern=pattern, disp=disp_offset, end=instr_end,
                                       lea=' lea ' in f' {text} ', text=text))
                break
    if not candidates: return None
    vtable = not any(s['w'] for s in secs if s['va'] <= target < s['va'] + s['vs'])
    candidates.sort(key=lambda c: (not (c['lea'] or not vtable), len(c['pattern'])))
    return candidates[0]

def fmt(p): return ' '.join('??' if b is None else f'{b:02X}' for b in p)

results = {}
with concurrent.futures.ThreadPoolExecutor(6) as pool:
    code_jobs = {pool.submit(code_signature, n, r): (n, r) for n, r in FIELDS if n in FUNCTIONS or n in CODE_KIND}
    data_jobs = {pool.submit(data_signature, n, r): (n, r) for n, r in FIELDS if n in DATA}
    for job in concurrent.futures.as_completed(list(code_jobs) + list(data_jobs)):
        n, r = code_jobs.get(job) or data_jobs.get(job)
        res = job.result()
        if n in PREFER_CALL and job in code_jobs: res = None  # entry is a protector thunk
        if n in RTTI: results[n] = dict(rva=r, kind='rtti', cls=RTTI[n]); continue
        if res is None and n in OPTIONAL: results[n] = dict(rva=r, kind='optional'); continue
        if res is None and n in FUNCTIONS:  # through a unique call to the function
            via = data_signature(n, r)
            if via and via['text'].lstrip().startswith('call'):
                results[n] = dict(rva=r, kind='data', pattern=fmt(via['pattern']), disp=via['disp'], end=via['end'], via=via['text'])
                print(n, 'ok via call', file=sys.stderr); continue
        if res is None: results[n] = None; print('NO SIGNATURE', n, file=sys.stderr); continue
        if job in code_jobs:
            pattern, offset = res
            results[n] = dict(rva=r, kind='code', pattern=fmt(pattern), offset=offset)
        else:
            results[n] = dict(rva=r, kind='data', pattern=fmt(res['pattern']), disp=res['disp'], end=res['end'], via=res['text'])
        print(n, 'ok', file=sys.stderr)
out = dict(exe=dict(stamp=struct.unpack_from('<I', d, pe + 8)[0], size=SIZE, checksum=struct.unpack_from('<I', d, opt + 64)[0]),
           code_section=code['name'], fields=[dict(name=n, **(results.get(n) or {})) for n, _ in FIELDS])
Path(__file__).with_name('signatures.json').write_text(json.dumps(out, indent=1))
print('missing:', [n for n, _ in FIELDS if not results.get(n)])

from pathlib import Path
import sys,json,struct
root=Path(__file__).resolve().parents[1]
from development_paths import use_toolchain
use_toolchain()
from core.compression_engine import _decompress_type1_par
from core.mesh_parser import _parse_par_sections,_find_pac_descriptors,_find_pac_section_layout,_detect_pac_vertex_stride,_read_pac_indices
def lod_layout(data):
    sections=_parse_par_sections(data)
    sec0=next(s for s in sections if s['index']==0)
    nl=data[sec0['offset']+4]
    descs=_find_pac_descriptors(data,sec0['offset'],sec0['size'],nl)
    assert descs
    layouts=[]
    geom=next(s for i in [4,3,2,1] for s in sections if s['index']==i)
    stride=_detect_pac_vertex_stride(data,geom['offset'],geom['offset']+geom['size']) or 40
    for sec in sections:
        if sec['index'] not in [1,2,3,4]:continue
        lod=4-sec['index']
        counts=[d.vertex_counts[lod] if lod<len(d.vertex_counts) else 0 for d in descs]
        icounts=[d.index_counts[lod] if lod<len(d.index_counts) else 0 for d in descs]
        if not sum(counts):continue
        start,ioff=_find_pac_section_layout(data,sec,descs,lod,sum(icounts),stride=stride)
        assert stride==40,stride
        assert start+sum(counts)*stride<=ioff,(start,counts,ioff)
        assert ioff+sum(icounts)*2<=sec['size']
        voff=start
        for di,(vc,ic) in enumerate(zip(counts,icounts)):
            indices=_read_pac_indices(data,sec['offset'],sec['size'],ioff,ic)
            assert len(indices)==ic
            assert not indices or max(indices)<vc,('alias',di,vc,max(indices))
            layouts.append({'lod':lod,'di':di,'vc':vc,'ic':ic,'voff':sec['offset']+voff,'ioff':sec['offset']+ioff,'stride':stride})
            voff+=vc*stride
            ioff+=ic*2
    return descs,layouts

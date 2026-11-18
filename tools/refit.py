from pathlib import Path
import sys, json, struct, hashlib, logging
import numpy as np
from scipy.spatial import cKDTree
from scipy.ndimage import gaussian_filter, map_coordinates

ROOT=Path(__file__).resolve().parents[1]
from development_paths import SOURCES,dependency,use_toolchain
use_toolchain()
from core.compression_engine import _decompress_type1_par
from core.mesh_parser import parse_mesh,_decode_pac_vertex_record,_compute_smooth_normals
from core.mesh_importer import _quantize_pac_u16,_patch_pac_descriptor_bounds,_pack_pac_normal,_compute_mesh_tangents,_pack_pac_tangent_into_record
from pac_lods import lod_layout
from shading_frames import decode_pac_tangent,transport_frames
from witch_parts import isolated_model

GAME=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert')
PROJECT=ROOT
AUDIT=SOURCES/'vanilla'

def sha(b):return hashlib.sha256(b).hexdigest()

def deformation():
    generic=next((AUDIT/'originals').rglob('phw_00_nude_00_0001.pac'))
    venus=dependency('venus')
    a=parse_mesh(_decompress_type1_par(generic.read_bytes()),generic.name)
    b=parse_mesh(_decompress_type1_par(venus.read_bytes()),venus.name)
    source=[];delta=[]
    for i,(s,t) in enumerate(zip(a.submeshes,b.submeshes)):
        assert s.faces==t.faces and s.uvs==t.uvs
        x=np.array(s.vertices)
        y=np.array(t.vertices)
        source.append(x)
        delta.append(y-x if i else np.zeros_like(x))
    source=np.concatenate(source)
    delta=np.concatenate(delta)
    tree=cKDTree(source)
    def sample(points):
        d,j=tree.query(points,k=12)
        w=1/np.maximum(d,0.001)**2
        w/=w.sum(axis=1)[:,None]
        shifts=(delta[j]*w[:,:,None]).sum(axis=1)
        falloff=np.exp(-(d[:,0]/0.18)**2)
        shifts*=falloff[:,None]
        return shifts
    lo=np.array([-0.65,-0.20,-0.65])
    step=0.02
    axes=[np.arange(lo[i],np.array([0.65,2.10,0.65])[i]+step/2,step) for i in range(3)]
    grid=np.stack(np.meshgrid(*axes,indexing='ij'),axis=-1)
    values=sample(grid.reshape(-1,3)).reshape(grid.shape)
    for i in range(3):values[:,:,:,i]=gaussian_filter(values[:,:,:,i],sigma=0.8)
    def apply(points):
        if not len(points):return points
        coords=((points-lo)/step).T
        shifts=np.stack([map_coordinates(values[:,:,:,i],coords,order=1,mode='nearest') for i in range(3)],axis=-1)
        return points+shifts
    return apply,{'source_body_sha256':sha(generic.read_bytes()),'venus_sha256':sha(venus.read_bytes()),'method':'12-neighbour inverse-distance body delta, 20 mm grid and 16 mm Gaussian smoothing; 180 mm distance falloff; head delta zero; all stored LODs','max_control_delta_m':float(np.linalg.norm(delta,axis=1).max())}

def build_one(source,dest,field,shading='recalculate'):
    if shading not in {'preserve','transport','recalculate'}:raise ValueError('Unknown shading mode')
    source,dest=Path(source),Path(dest)
    if dest.exists():raise FileExistsError('Choose a fresh output; existing or linked model files are never overwritten.')
    if source.resolve()==dest.resolve():raise ValueError('Source and output must be separate files.')
    original=source.read_bytes()
    data=_decompress_type1_par(original)
    descs,layouts=lod_layout(data)
    decoded=[]
    stabilized=0
    for layout in layouts:
        desc=descs[layout['di']]
        recs=[_decode_pac_vertex_record(data,layout['voff']+i*layout['stride'],desc) for i in range(layout['vc'])]
        p=np.array([r[0] for r in recs],dtype=float).reshape((-1,3))
        requested=field(p) if len(p) else p
        q=requested.copy()
        assert np.isfinite(q).all()
        ids=struct.unpack_from('<'+'H'*layout['ic'],data,layout['ioff'])
        faces=[tuple(ids[i:i+3]) for i in range(0,len(ids),3)]
        assert all(len(f)==3 for f in faces)
        if faces:
            f=np.asarray(faces)
            n0=np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]])
            area=np.linalg.norm(n0,axis=1)
            for _ in range(30):
                n1=np.cross(q[f[:,1]]-q[f[:,0]],q[f[:,2]]-q[f[:,0]])
                bad=np.where((area>1e-9)&((n0*n1).sum(axis=1)<0.1*area*area))[0]
                if not len(bad):break
                stabilized+=len(bad)
                for fi in bad:
                    vv=f[fi]
                    shift=(q[vv]-p[vv]).mean(axis=0)
                    q[vv]=p[vv]+shift
            else:raise ValueError('Could not stabilize folded LOD triangles')
        decoded.append((layout,recs,p,q,faces,np.linalg.norm(q-requested,axis=1)>1e-8))
    out=bytearray(data)
    permitted=bytearray(len(data))
    expected={}
    max_shift=0.0
    changed_vertices=0
    shading_changed=0
    stabilized_frames_retained=0
    differential_checks=[]
    for di,desc in enumerate(descs):
        group=[x for x in decoded if x[0]['di']==di]
        nonempty=[x for x in group if len(x[3])]
        if not nonempty:continue
        allp=np.concatenate([x[3] for x in nonempty])
        bmin=allp.min(axis=0).astype(np.float32)
        extent=(allp.max(axis=0)-bmin).astype(np.float32)
        _patch_pac_descriptor_bounds(out,desc.descriptor_offset,tuple(bmin),tuple(extent))
        permitted[desc.descriptor_offset+11:desc.descriptor_offset+35]=b'\x01'*24
        for layout,recs,p,q,faces,stabilized_frames in group:
            if not len(q):continue
            shifts=np.linalg.norm(q-p,axis=1)
            max_shift=max(max_shift,float(shifts.max()))
            changed_vertices+=int((shifts>0.00001).sum())
            if shading=='recalculate':
                normals=_compute_smooth_normals(q.tolist(),faces)
                uvs=[r[1] for r in recs]
                tangents,signs,valid=_compute_mesh_tangents(q.tolist(),uvs,normals,faces)
                valid=np.asarray(valid,dtype=bool)
                rewrite=np.full(len(q),shifts.max()>0.000001)
            elif shading=='transport':
                source_frames=[decode_pac_tangent(data[layout['voff']+i*40:layout['voff']+(i+1)*40]) for i in range(len(p))]
                normals,tangents,rewrite,check=transport_frames(p,[r[2] for r in recs],[frame[0] for frame in source_frames],field)
                signs=[frame[1] for frame in source_frames];valid=np.ones(len(p),dtype=bool)
                rewrite&=~stabilized_frames
                stabilized_frames_retained+=int(stabilized_frames.sum())
                differential_checks.append(check)
            else:rewrite=np.zeros(len(q),dtype=bool)
            for i in range(len(q)):
                pos=layout['voff']+i*40
                rec=bytearray(data[pos:pos+40])
                struct.pack_into('<HHH',rec,0,*[_quantize_pac_u16(q[i,k],bmin[k],extent[k]) for k in range(3)])
                if rewrite[i]:
                    existing=struct.unpack_from('<I',rec,16)[0]
                    struct.pack_into('<I',rec,16,_pack_pac_normal(normals[i],existing))
                    if valid[i]:_pack_pac_tangent_into_record(rec,tangents[i],signs[i])
                assert rec[8:16]==data[pos+8:pos+16]
                assert rec[20:]==data[pos+20:pos+40]
                if shading=='preserve':assert rec[6:8]==data[pos+6:pos+8] and rec[16:20]==data[pos+16:pos+20]
                if shading=='transport':assert (struct.unpack_from('<I',rec,16)[0]^struct.unpack_from('<I',data,pos+16)[0])&0x80000000==0
                shading_changed+=rec[6:8]!=data[pos+6:pos+8] or rec[16:20]!=data[pos+16:pos+20]
                out[pos:pos+40]=rec
                permitted[pos:pos+6]=b'\x01'*6
                if shading!='preserve':
                    permitted[pos+6:pos+8]=b'\x01'*2
                    permitted[pos+16:pos+20]=b'\x01'*4
            expected[(di,layout['lod'])]=q
    assert all(a==b or permitted[i] for i,(a,b) in enumerate(zip(data,out)))
    assert len(data)==len(out)
    round_descs,round_layouts=lod_layout(bytes(out))
    assert layouts==round_layouts
    assert len(descs)==len(round_descs)
    max_err=0.0
    inversions=0
    for layout,recs,p,q,faces,_ in decoded:
        desc=round_descs[layout['di']]
        actual=np.array([_decode_pac_vertex_record(out,layout['voff']+i*40,desc)[0] for i in range(layout['vc'])]).reshape((-1,3))
        if len(actual):max_err=max(max_err,float(np.max(np.abs(q-actual))))
        before=data[layout['ioff']:layout['ioff']+layout['ic']*2]
        after=bytes(out[layout['ioff']:layout['ioff']+layout['ic']*2])
        assert before==after
        if faces:
            f=np.array(faces)
            n0=np.cross(p[f[:,1]]-p[f[:,0]],p[f[:,2]]-p[f[:,0]])
            n1=np.cross(actual[f[:,1]]-actual[f[:,0]],actual[f[:,2]]-actual[f[:,0]])
            good=(np.linalg.norm(n0,axis=1)>1e-9)&(np.linalg.norm(n1,axis=1)>1e-9)
            inversions+=int(((n0*n1).sum(axis=1)[good]<0).sum())
    assert max_err<0.00015,max_err
    assert inversions==0,('triangle flips',inversions)
    parsed=parse_mesh(bytes(out),source.name)
    original_parsed=parse_mesh(data,source.name)
    assert parsed.total_vertices==original_parsed.total_vertices
    assert parsed.total_faces==original_parsed.total_faces
    dest.parent.mkdir(exist_ok=True,parents=True)
    with dest.open('xb') as output:output.write(out)
    return {'model':source.name,'source_sha256':sha(original),'output_sha256':sha(out),'source_bytes':len(original),'output_bytes':len(out),'lods':sorted(set(x['lod'] for x in layouts)),'lod_vertex_records':sum(x['vc'] for x in layouts),'lod0_vertices':parsed.total_vertices,'lod0_faces':parsed.total_faces,'changed_vertex_records':changed_vertices,'max_displacement_m':max_shift,'max_quantization_error_m':max_err,'triangle_normal_flips':inversions,'stabilized_triangle_updates':stabilized,'skin_uv_indices_preserved':True,'unrelated_bytes_preserved':True,'shading_mode':shading,'shading_records_changed':int(shading_changed),'authored_frames_retained_at_stabilized_vertices':stabilized_frames_retained,'differential_checks':differential_checks}

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Build fresh offline default-outfit refit candidates from external sources.')
    parser.add_argument('--output',type=Path,default=PROJECT/'Hex Appeal - Outfit Refits'/'files')
    parser.add_argument('--report',type=Path,default=PROJECT/'reports'/'refit-validation.json')
    parser.add_argument('--shading',choices=['preserve','transport','recalculate'],default='recalculate')
    args=parser.parse_args()
    if args.output.exists() or args.report.exists():parser.error('Choose a fresh --output and --report; previous refits and their evidence are preserved.')
    field,method=deformation()
    report=[]
    for p in sorted((AUDIT/'witch-sources'/'0009').rglob('*.pac')):
        rel=p.relative_to(AUDIT/'witch-sources')
        dest=args.output/isolated_model(rel.as_posix())
        try:
            r=build_one(p,dest,field,args.shading)
            r['path']=rel.as_posix()
            report.append(r)
            print(p.name,'OK',r['changed_vertex_records'],'vertices; flips',r['triangle_normal_flips'],flush=True)
        except Exception as ex:
            print(p.name,'FAILED',repr(ex),flush=True)
            report.append({'path':rel.as_posix(),'error':repr(ex)})
    args.report.parent.mkdir(exist_ok=True,parents=True)
    with args.report.open('x',encoding='utf-8') as output:output.write(json.dumps({'method':method,'models':report,'in_game_tested':False},indent=2)+'\n')
    print('BUILT',sum('error' not in x for x in report),'FAILED',sum('error' in x for x in report),flush=True)

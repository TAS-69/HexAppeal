"""Validate a Wardrobe definition and emit original C++ registration code."""
from pathlib import Path
import argparse, itertools, json, re

def text(value, capacity, label, empty=False):
    if not isinstance(value,str) or (not empty and not value) or len(value.encode('utf-8'))>=capacity or any(ord(c)<32 or ord(c)==127 for c in value):
        raise ValueError(f'{label}: provide text shorter than {capacity} UTF-8 bytes without control characters')
    return value

def identifier(value, capacity, label, outfit=False):
    text(value,capacity,label)
    if not re.fullmatch(r'[a-z0-9_./-]+' if outfit else r'[a-z0-9_.-]+',value):
        raise ValueError(f'{label}: invalid identifier')
    if outfit and ('/' not in value or value.startswith('hexappeal/')):
        raise ValueError(f'{label}: use your own provider namespace')
    return value

def prefab(value,label):
    text(value,128,label)
    if not re.fullmatch(r'[A-Za-z0-9_]+',value):
        raise ValueError(f'{label}: use a registered prefab name, without a file path or extension')
    return value

def validate(data):
    if not isinstance(data,dict) or type(data.get('schema')) is not int or data.get('schema')!=1:
        raise ValueError('schema must be 1')
    allowed={'schema','id','label','prefab','credit','requirements','controls','native_controls','states','part_visibility','replace_base_underwear'}
    if set(data)-allowed:raise ValueError('Unknown outfit fields: '+', '.join(sorted(set(data)-allowed)))
    out={key:text(data.get(key,''),size,key,empty=key in {'credit','requirements'}) for key,size in [('id',96),('label',64),('credit',128),('requirements',256)]}
    identifier(out['id'],96,'id',True);out['prefab']=prefab(data.get('prefab'),'prefab')
    replace=data.get('replace_base_underwear',False)
    if type(replace) is not bool:raise ValueError('replace_base_underwear must be a boolean')
    out['replace_base_underwear']=replace
    raw=data.get('controls',[])
    if not isinstance(raw,list) or len(raw)>8:raise ValueError('Declare at most eight controls')
    controls=[];ids=set();variant_count=0
    for number,c in enumerate(raw):
        if not isinstance(c,dict) or set(c)-{'id','label','help','type','scope','default','options','prefab_variant','follow_base_underwear','follow_outfit_hide'}:
            raise ValueError(f'controls[{number}]: unknown fields')
        control={'id':identifier(c.get('id'),32,'control id'),'label':text(c.get('label'),64,'control label'),
                 'help':text(c.get('help',''),256,'control help',True),'type':c.get('type'),'scope':c.get('scope','per_witch')}
        if control['id'] in ids:raise ValueError('Duplicate control id: '+control['id'])
        ids.add(control['id'])
        if control['type'] not in {'toggle','choice'} or control['scope'] not in {'per_witch','global'}:raise ValueError('Invalid control type or scope')
        follow=c.get('follow_base_underwear',False);variant=c.get('prefab_variant',False);nude=c.get('follow_outfit_hide',False)
        if not isinstance(follow,bool) or not isinstance(variant,bool) or not isinstance(nude,bool):raise ValueError('Control flags must be booleans')
        control['flags']=(2 if follow else 0)|(1 if variant else 0)|(4 if nude else 0)
        if follow and (control['type']!='toggle' or control['scope']!='per_witch'):raise ValueError('Base underwear applies to per-witch visibility toggles only')
        if nude and (control['type']!='toggle' or control['scope']!='per_witch'):raise ValueError('Hide outfit applies to per-witch visibility toggles only')
        if follow and nude:raise ValueError('Keep clothing and base underwear on separate controls')
        if control['type']=='toggle':
            if variant or c.get('options') or not isinstance(c.get('default',False),bool):raise ValueError('Toggle defaults must be booleans; toggles have no options or prefab_variant flag')
            control.update(default=int(c.get('default',False)),options=[])
        else:
            options=c.get('options')
            if not isinstance(options,list) or not 1<=len(options)<=16:raise ValueError('Choices require one to sixteen options')
            parsed=[];option_ids=set()
            for option in options:
                if not isinstance(option,dict) or set(option)-{'id','label','prefab'}:raise ValueError('Invalid choice option fields')
                oid=identifier(option.get('id'),32,'option id')
                if oid in option_ids:raise ValueError('Duplicate option id')
                option_ids.add(oid)
                name=prefab(option.get('prefab'),'option prefab') if variant else ''
                if not variant and option.get('prefab'):raise ValueError('Only prefab_variant options have individual prefabs')
                parsed.append({'id':oid,'label':text(option.get('label'),64,'option label'),'prefab':name})
            default=c.get('default',parsed[0]['id'])
            if default not in option_ids:raise ValueError('Unknown default choice: '+str(default))
            control.update(default=next(i for i,o in enumerate(parsed) if o['id']==default),options=parsed)
            if variant:
                if control['scope']!='per_witch':raise ValueError('Prefab variants must be per witch')
                variant_count+=1
        controls.append(control)
    if variant_count>1:raise ValueError('Use native_controls for combined variants')
    native=data.get('native_controls',[])
    if not isinstance(native,list) or not all(isinstance(i,str) for i in native) or len(set(native))!=len(native) or any(i not in ids for i in native):raise ValueError('native_controls must name unique declared controls')
    mask=0;positions=[]
    for i,c in enumerate(controls):
        if c['id'] in native:
            if c['scope']!='per_witch':raise ValueError('Native combinations cannot alter global shared resources')
            mask|=1<<i;positions.append(i)
    states=[]
    if native:
        if variant_count:raise ValueError('native_controls replaces the single prefab_variant flag')
        dimensions=[2 if controls[i]['type']=='toggle' else len(controls[i]['options']) for i in positions]
        count=1
        for dimension in dimensions:
            count*=dimension
            if count>256:raise ValueError('Native combinations exceed 256 states')
        expected=set(itertools.product(*(range(size) for size in dimensions)))
        raw_states=data.get('states',[])
        if not isinstance(raw_states,list) or len(raw_states)!=len(expected):raise ValueError('Supply every native combination exactly once')
        seen=set()
        for state in raw_states:
            if not isinstance(state,dict) or set(state)!={'values','prefab'} or not isinstance(state['values'],dict) or set(state['values'])!=set(native):raise ValueError('State values must specify exactly the native controls')
            values=[0]*8
            for i in positions:
                c=controls[i];v=state['values'][c['id']]
                if c['type']=='toggle':
                    if not isinstance(v,bool):raise ValueError('State toggle values must be booleans')
                    values[i]=int(v)
                else:
                    if not isinstance(v,str) or v not in [o['id'] for o in c['options']]:raise ValueError('Unknown state choice')
                    values[i]=next(j for j,o in enumerate(c['options']) if o['id']==v)
            key=tuple(values[i] for i in positions)
            if key in seen:raise ValueError('Duplicate native combination')
            seen.add(key);states.append({'values':values,'prefab':prefab(state['prefab'],'state prefab')})
        if seen!=expected:raise ValueError('Incomplete native combinations')
        default=next(s for s in states if all(s['values'][i]==controls[i]['default'] for i in positions))
        if default['prefab']!=out['prefab']:raise ValueError('prefab must match the default native combination')
    elif data.get('states'):raise ValueError('states requires native_controls')
    raw_parts=data.get('part_visibility',[])
    if not isinstance(raw_parts,list) or len(raw_parts)>16:raise ValueError('Declare at most sixteen visibility parts')
    parts=[];part_names=set()
    for item in raw_parts:
        if not isinstance(item,dict) or set(item)!={'control','part'}:raise ValueError('Visibility bindings require control and part')
        control=next((c for c in controls if c['id']==item['control']),None)
        if not control or control['type']!='toggle' or control['scope']!='per_witch':raise ValueError('Visibility parts require declared per-witch toggles')
        if control['id'] in native:raise ValueError('A toggle cannot drive both prefab combinations and visibility parts')
        part=text(item['part'],64,'visibility part')
        if not re.fullmatch(r'[A-Za-z0-9_]+',part) or part.lower() in {'underwear','nude','body','head','face'}:raise ValueError('Use an outfit-owned visibility part; base body parts are reserved')
        if part in part_names:raise ValueError('Duplicate visibility part')
        part_names.add(part);parts.append({'control':control['id'],'part':part})
    out.update(controls=controls,native_mask=mask,states=states,parts=parts)
    return out

def render(out):
    literal=lambda value:json.dumps(value,ensure_ascii=False)
    symbol='Register_'+re.sub(r'[^A-Za-z0-9_]','_',out['id'])
    lines=['#pragma once','#include "HexAppealWardrobeClient.hpp"','',f'inline int {symbol}(hexappeal::WardrobeClient& client,uint64_t& token){{',f'    std::array<HA_Control,{len(out["controls"])}> controls{{}};']
    if out['replace_base_underwear']:lines.append('    if(!client.HasOutfitPolicy())return HA_UNAVAILABLE;')
    for i,c in enumerate(out['controls']):
        lines += [f'    controls[{i}]=hexappeal::Sized<HA_Control>();',f'    controls[{i}].type={"HA_TOGGLE" if c["type"]=="toggle" else "HA_CHOICE"};',f'    controls[{i}].scope={"HA_GLOBAL" if c["scope"]=="global" else "HA_PER_WITCH"};',f'    controls[{i}].flags={c["flags"]}; controls[{i}].defaultValue={c["default"]};']
        for name in ['id','label','help']:lines.append(f'    hexappeal::Text(controls[{i}].{name},{literal(c[name])});')
        if c['options']:
            lines.append(f'    std::array<HA_Option,{len(c["options"])}> options_{i}{{}};')
            for j,o in enumerate(c['options']):
                for name in ['id','label','prefab']:lines.append(f'    hexappeal::Text(options_{i}[{j}].{name},{literal(o[name])});')
            lines.append(f'    controls[{i}].options=options_{i}.data(); controls[{i}].optionCount={len(c["options"])};')
    lines += ['    auto outfit=hexappeal::Sized<HA_Outfit>();',f'    outfit.controlCount={len(out["controls"])}; outfit.controls=controls.data();']
    for name in ['id','label','prefab','credit','requirements']:lines.append(f'    hexappeal::Text(outfit.{name},{literal(out[name])});')
    if out['native_mask']:
        lines.append(f'    std::array<HA_PrefabState,{len(out["states"])}> states{{}};')
        for i,state in enumerate(out['states']):
            lines.append(f'    states[{i}]=hexappeal::Sized<HA_PrefabState>();')
            for j,value in enumerate(state['values']):
                if value:lines.append(f'    states[{i}].values[{j}]={value};')
            lines.append(f'    hexappeal::Text(states[{i}].prefab,{literal(state["prefab"])});')
    if out['parts']:
        lines.append(f'    std::array<HA_VisibilityPart,{len(out["parts"])}> parts{{}};')
        for i,part in enumerate(out['parts']):
            lines.append(f'    parts[{i}]=hexappeal::Sized<HA_VisibilityPart>();')
            for name in ['control','part']:lines.append(f'    hexappeal::Text(parts[{i}].{name},{literal(part[name])});')
    if out['native_mask'] or out['parts']:
        lines += ['    auto native=hexappeal::Sized<HA_NativeOutfit>();','    native.outfit=&outfit;']
        if out['native_mask']:lines += [f'    native.controlMask={out["native_mask"]}; native.stateCount={len(out["states"])};','    native.states=states.data();']
        if out['parts']:lines += [f'    native.partCount={len(out["parts"])};','    native.parts=parts.data();']
        call='client.RegisterNative(native,token)'
    else:call='client.Register(outfit,token)'
    if out['replace_base_underwear']:
        lines += [f'    const auto result={call};','    if(result!=HA_OK)return result;',
                  '    const auto policy=client.Policy(token,HA_REPLACE_BASE_UNDERWEAR);',
                  '    if(policy!=HA_OK){client.Unregister(token);return policy;}','    return HA_OK;']
    else:lines.append(f'    return {call};')
    lines += ['}','']
    return '\n'.join(lines)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('definition',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--force',action='store_true')
    args=parser.parse_args()
    if args.definition.stat().st_size>1048576:raise ValueError('Definition exceeds 1 MiB')
    out=validate(json.loads(args.definition.read_text(encoding='utf-8-sig')))
    if args.output:
        target=args.output.resolve()
        if target==args.definition.resolve():raise ValueError('Output must be separate from the JSON definition')
        if target.exists() and (not args.force or target.stat().st_nlink>1):raise ValueError('Output exists or is linked; choose a fresh path')
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(render(out),encoding='utf-8')
    print(json.dumps({'valid':True,'outfit':out['id'],'controls':len(out['controls']),'native_states':len(out['states']),'output':str(args.output) if args.output else None}))

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,TypeError,KeyError) as error:raise SystemExit('Invalid Wardrobe definition: '+str(error))

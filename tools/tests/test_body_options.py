from pathlib import Path
import sys, unittest, xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from body_options import BODY_MODELS, UNDERWEAR_MODELS, body_registration, body_lookup, private_body, vanilla_patch,validate_body_resources,isolate_underwear
from body_options import UNDERWEAR_TEXTURES, isolate_underwear_material
from part_registry import write_registry


class BodyOptions(unittest.TestCase):
    def test_each_original_body_and_proportion_survives_the_vanilla_option(self):
        for name in BODY_MODELS:
            source=f'<Appearance><Customization CustomizationFile="personal"/><Nude><Prefab Name="{name}" CharacterScale="0.97" EnableCharacterScaleLimit="false"/></Nude><Head><Prefab Name="original_head"/></Head><Armor><Prefab Name="cd_phw_00_foot_00_0160"/></Armor></Appearance>'
            patch, body=vanilla_patch(source.encode())
            self.assertEqual(body,{'Name':name,'CharacterScale':'0.97','EnableCharacterScaleLimit':'false'})
            operations=ET.fromstring(patch)
            self.assertEqual(operations[0].get('value'),private_body(name))
            self.assertEqual(operations[1].get('value'),'ha_phw_00_foot_00_0160')
            self.assertTrue(all(operation.get('attr')=='Name' for operation in operations))

    def test_body_alias_changes_one_model_reference_and_keeps_underwear(self):
        for name,model in BODY_MODELS.items():
            registry={'primary':[{'name':name,'directory':'1_pc/02_phw/nude','children':[('Nude',1),('Underwear',1)]}]}
            path='character/model/1_pc/2_phw/nude/'+model+'.pac'
            underwear=b'character/model/1_pc/2_phw/armor/38_underwear/phw_00_uw_00_0001.pac'
            before=b'prefix\0'+path.encode()+b'\0'+underwear+b'\0suffix'
            row,source,target,after=body_registration(registry,name,before)
            self.assertEqual(len(before),len(after))
            self.assertEqual(after,before.replace(source.encode(),target.encode()))
            self.assertIn(underwear,after)
            self.assertEqual(row['children'],[('Nude',1),('Underwear',1)])
            self.assertEqual(registry['primary'][0]['name'],name)

    def test_unknown_body_and_ambiguous_prefab_do_not_produce_output(self):
        with self.assertRaises(ValueError):
            vanilla_patch(b'<Appearance><Nude><Prefab Name="unknown"/></Nude></Appearance>')
        name='phw_00_nude_00_0001'
        registry={'primary':[{'name':name,'directory':'1_pc/02_phw/nude'}]}
        with self.assertRaises(ValueError):
            body_registration(registry,name,b'unknown source layout')

    def test_underwear_is_isolated_without_replacing_shared_damiane_files(self):
        for name,model in UNDERWEAR_MODELS.items():
            original='character/model/1_pc/2_phw/armor/38_underwear/'+model+'.pac'
            private=original.replace('/1_pc/','/ha_p/').replace('/phw_','/hvw_')
            before=b'Underwear\0'+original.encode()+b'\0Nude'
            after=isolate_underwear(before,name)
            self.assertEqual(len(before),len(after))
            self.assertEqual(after,before.replace(original.encode(),private.encode()))
            with self.assertRaises(ValueError):isolate_underwear(before+before,name)

    def test_body_cache_lookup_is_required_alongside_primary_registration(self):
        for name in BODY_MODELS:
            self.assertEqual(body_lookup({'secondary':[(name,'1_pc/02_phw/nude')]},name),(private_body(name),'ha_p/02_phw/nude'))
            with self.assertRaises(ValueError):
                body_lookup({'secondary':[]},name)
            with self.assertRaises(ValueError):
                body_lookup({'secondary':[(name,'wrong/path')]},name)

    def test_vanilla_underwear_keeps_its_stock_texture_bindings(self):
        material = ''.join(f'<Texture _path="{path}"/>' for path in UNDERWEAR_TEXTURES)
        material += '<Texture _path="character/texture/phw_00_uw_00_0001_disp.dds"/>'
        expected = material
        for source, target in UNDERWEAR_TEXTURES.items():
            expected = expected.replace(source, target)
        self.assertEqual(isolate_underwear_material(material.encode()), expected.encode())
        with self.assertRaises(ValueError):
            isolate_underwear_material(b'<Texture/>')

    def test_each_body_definition_and_resource_is_required_before_packaging(self):
        rows=[];lookups=[];files={}
        for name,model in BODY_MODELS.items():
            alias=private_body(name)
            rows.append({'name':alias,'directory':'ha_p/02_phw/nude','socket':'','flag':1,'asset':'','flag2':0,'children':[('Nude',1),('Underwear',1)]})
            lookups.append((alias,'ha_p/02_phw/nude'))
            path='character/model/ha_p/2_phw/nude/hvw'+model[3:]+'.pac'
            files['files/0009/'+path]=b'model test input'
            files['files/0009/'+path.replace('character/model/','character/modelproperty/')+'_xml']=b'property test input'
            files['files/0009/'+path.replace('character/model/','character/bin__/meshphysics/').removesuffix('.pac')+'.hkx']=b'physics test input'
            underwear='character/model/ha_p/2_phw/armor/38_underwear/hvw'+UNDERWEAR_MODELS[name][3:]+'.pac'
            files['files/0009/'+underwear]=b'original underwear model'
            files['files/0009/'+underwear.replace('character/model/','character/modelproperty/')+'_xml']=b'original underwear properties'
            files['files/0009/'+underwear.replace('character/model/','character/bin__/meshphysics/').removesuffix('.pac')+'.hkx']=b'original underwear physics'
            files['files/0009/character/bin__/prefab/ha_p/02_phw/nude/'+alias+'.prefab']=path.encode()+b'\0'+underwear.encode()
            files['files/0009/character/prefab/ha_p/02_phw/nude/'+alias+'.prefabdata_xml']=b'<NudePrefabData><BaseCharacterScale Value="0.97"/><SkeletonName FileName="original.pab"/></NudePrefabData>'
        files['files/0009/character/bin__/partprefabtable.pappt']=write_registry({'header':bytes(8),'primary':rows,'secondary':lookups})
        self.assertEqual(validate_body_resources(files),21)
        for path in list(files):
            if path.endswith('.pappt'):continue
            incomplete=dict(files);del incomplete[path]
            with self.subTest(path=path),self.assertRaises(ValueError):
                validate_body_resources(incomplete)


if __name__=='__main__':
    unittest.main()

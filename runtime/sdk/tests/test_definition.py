from pathlib import Path
import copy, importlib.util, json, unittest

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('generate_provider',root/'tools/generate_provider.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class DefinitionTests(unittest.TestCase):
    def setUp(self):self.definition=json.loads((root/'examples/outfit.json').read_text())
    def invalid(self,edit):
        edit(self.definition)
        with self.assertRaises((ValueError,TypeError)):module.validate(self.definition)
    def test_complete_visibility_states(self):
        result=module.validate(self.definition)
        self.assertEqual(result['native_mask'],3);self.assertEqual(len(result['states']),4)
        header=module.render(result)
        self.assertIn('client.RegisterNative(native,token)',header)
        self.assertNotIn('SetMyOutfit',header)
    def test_old_provider_without_native_states(self):
        self.definition.pop('states');self.definition.pop('native_controls')
        result=module.validate(self.definition)
        self.assertIn('client.Register(outfit,token)',module.render(result))
    def test_reserved_namespace(self):self.invalid(lambda d:d.update(id='hexappeal/dancer'))
    def test_utf8_byte_limits(self):self.invalid(lambda d:d.update(label='é'*32))
    def test_no_injected_source(self):
        self.definition['label']='A "quoted" dress \\ name'
        self.assertIn('\\"quoted\\"',module.render(module.validate(self.definition)))
    def test_control_characters(self):self.invalid(lambda d:d.update(label='My\nDress'))
    def test_unknown_fields(self):self.invalid(lambda d:d.update(native_control=['bra']))
    def test_duplicate_controls(self):self.invalid(lambda d:d['controls'].append(copy.deepcopy(d['controls'][0])))
    def test_missing_combination(self):self.invalid(lambda d:d['states'].pop())
    def test_duplicate_combination(self):self.invalid(lambda d:d['states'].__setitem__(0,copy.deepcopy(d['states'][1])))
    def test_incorrect_default(self):self.invalid(lambda d:d.update(prefab='some_other_prefab'))
    def test_file_path_is_not_prefab(self):self.invalid(lambda d:d['states'][0].update(prefab='character/dress.prefab'))
    def test_global_native_control_rejected(self):self.invalid(lambda d:d['controls'][0].update(scope='global',follow_base_underwear=False))
    def test_global_underwear_rejected(self):self.invalid(lambda d:d['controls'][0].update(scope='global'))
    def test_integer_toggle_rejected(self):self.invalid(lambda d:d['states'][0]['values'].update(bra=1))
    def test_unknown_state_key(self):self.invalid(lambda d:d['states'][0]['values'].update(other=False))
    def test_huge_product_rejected_before_enumeration(self):
        self.definition['controls']=[{'id':f'choice{i}','label':'Choice','type':'choice','options':[{'id':f'o{j}','label':str(j)} for j in range(16)]} for i in range(8)]
        self.definition['native_controls']=[c['id'] for c in self.definition['controls']]
        with self.assertRaisesRegex(ValueError,'256'):module.validate(self.definition)
    def test_schema_boolean_rejected(self):self.invalid(lambda d:d.update(schema=True))
    def parts(self):
        self.definition.pop('native_controls');self.definition.pop('states')
        self.definition['part_visibility']=[{'control':'bra','part':'MyBra'},{'control':'panties','part':'MyBriefs'}]
    def test_native_visibility_parts(self):
        self.parts();result=module.validate(self.definition)
        self.assertEqual(result['native_mask'],0);self.assertEqual(len(result['parts']),2)
        self.assertIn('native.parts=parts.data()',module.render(result))
        self.assertNotIn('native.states=',module.render(result))
    def test_visibility_cannot_use_base_body_keys(self):
        self.parts();self.invalid(lambda d:d['part_visibility'][0].update(part='underwear'))
    def test_visibility_rejects_duplicate_parts(self):
        self.parts();self.invalid(lambda d:d['part_visibility'][1].update(part='MyBra'))
    def test_visibility_requires_declared_toggle(self):
        self.parts();self.invalid(lambda d:d['part_visibility'][0].update(control='unknown'))
    def test_visibility_rejects_mixed_ownership(self):
        self.invalid(lambda d:d.update(part_visibility=[{'control':'bra','part':'MyBra'}]))

if __name__=='__main__':unittest.main()

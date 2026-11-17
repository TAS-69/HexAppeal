from pathlib import Path
import importlib.util,itertools,json,re,unittest

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('advanced_generator',root/'tools/generate_provider.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class AdvancedExamples(unittest.TestCase):
    def definition(self,name):return json.loads((root/'examples'/name).read_text(encoding='utf-8'))
    def test_matrix_and_separate_owned_part(self):
        parsed=module.validate(self.definition('advanced-outfit.json'))
        self.assertEqual(parsed['native_mask'],7);self.assertEqual(len(parsed['states']),8)
        self.assertEqual({tuple(s['values'][:3]) for s in parsed['states']},set(itertools.product(range(2),repeat=3)))
        self.assertEqual(parsed['parts'],[{'control':'belt','part':'ExampleStudioBelt'}])
    def test_all_colours_have_complete_hidden_states(self):
        data=self.definition('advanced-outfit.json')
        for colour in ('black','white'):
            state=next(s for s in data['states'] if s['values']=={'colour':colour,'bra':False,'briefs':False})
            self.assertTrue(state['prefab'].endswith('_bare'))
    def test_missing_variant_fails_before_registration(self):
        data=self.definition('advanced-outfit.json');data['states'].pop()
        with self.assertRaisesRegex(ValueError,'every native combination'):module.validate(data)
    def test_renderer_owns_shared_colour(self):
        parsed=module.validate(self.definition('renderer-outfit.json'))
        self.assertEqual(parsed['native_mask'],0);self.assertEqual(parsed['controls'][0]['scope'],'global')
        self.assertTrue(all(c['flags']==2 for c in parsed['controls'][1:]))
    def test_documented_definitions_validate(self):
        guide=(root/'ADVANCED-EXAMPLES.md').read_text(encoding='utf-8')
        blocks=re.findall(r'```json\n(.*?)\n```',guide,re.S)
        self.assertEqual(len(blocks),2)
        for block in blocks:module.validate(json.loads(block))
    def test_documented_cpp_matches_compiled_sources(self):
        guide=(root/'ADVANCED-EXAMPLES.md').read_text(encoding='utf-8')
        for name in ('AdvancedOutfitProvider.cpp','CustomRendererProvider.cpp'):
            self.assertIn((root/'examples'/name).read_text(encoding='utf-8').strip(),guide)

if __name__=='__main__':unittest.main()

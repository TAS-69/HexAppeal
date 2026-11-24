import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import setup_witch_skin as setup


class WitchSkinSetupTests(unittest.TestCase):
    def test_face_match_targets_skin_albedo_in_game_xml_fragment(self):
        face = (b'\xef\xbb\xbf<SkinnedMeshPropertyCommon/><ModelPropertyList>'
                b'<Material _materialName="SkinnedMeshSkinWrinkle">'
                b'<MaterialParameterTexture _name="_baseColorTexture"><ResourceReferencePath_ITexture '
                b'_path="character/texture/face.dds"/></MaterialParameterTexture>'
                b'<MaterialParameterTexture _name="_normalTexture"><ResourceReferencePath_ITexture '
                b'_path="character/texture/face_n.dds"/></MaterialParameterTexture>'
                b'</Material></ModelPropertyList>')
        self.assertEqual(setup.face_colour(face), "character/texture/face.dds")
        with self.assertRaises(ValueError):
            setup.face_colour(face.replace(b"face.dds", b"../face.dds"))
        with self.assertRaises(ValueError):
            setup.face_colour(face.replace(b"_normalTexture", b"_baseColorTexture"))

    def test_face_route_is_independent_of_the_witch_body(self):
        look = b'<Appearance><Head><Prefab Name="PHW_00_Head_00_0141"/></Head><Nude><Prefab Name="phw_00_nude_00_0001_damian_lnw"/></Nude></Appearance>'
        self.assertEqual(setup.appearance_head(look)[1], "PHW_00_Head_00_0141")
        vanilla = look.replace(b"phw_00_nude_00_0001_damian_lnw", b"hvw_00_nude_00_0001")
        self.assertEqual(setup.appearance_head(vanilla)[1], "PHW_00_Head_00_0141")
        with self.assertRaises(ValueError):
            setup.appearance_head(look.replace(b"PHW_00_Head_00_0141", b"another_head"))

    def test_only_brightness_bytes_change(self):
        stock = (b'<CharacterCustomization Version="0">\r\n<DecorationParam>\r\n'
                 b'<Data UIKey="skinBrightness" Value="50"/>\r\n'
                 b'<Data UIKey="skinColor" Value="1"/>\r\n</>\r\n<MeshParam>face</>\r\n</>')
        changed = setup.apply_brightness(stock, 98)
        self.assertEqual(changed, stock.replace(b'Value="50"', b'Value="98"', 1))
        self.assertEqual(int(setup.brightness_field(changed)[1]), 98)
        with self.assertRaises(ValueError):
            setup.brightness_field(stock.replace(b'<DecorationParam>', b'<DecorationParam>\n'
                                                  b'<Data UIKey="skinBrightness" Value="25"/>'))

    def test_foreign_customization_and_partial_bari_are_skipped(self):
        items = json.loads((setup.ROOT / "build-data" / "witch-assets.json").read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temp:
            paths = {}
            for item in items:
                key = item["appearance"]
                witch, descriptor = setup.APPEARANCES[Path(key).stem]
                if key.endswith("cd_nhw_north_20026.app_xml"):
                    continue
                if witch == "Sylvia":
                    descriptor = "cd_pc/cd_phw_damian"
                path = Path(temp) / Path(key).name
                path.write_text(f'<Appearance><Customization CustomizationFile="{descriptor}_customization"/></Appearance>')
                paths[key] = path
            rows, skipped = setup.appearance_rows(None, paths)
            self.assertEqual({row[0] for row in rows}, {"Areciel", "Elowen", "Lyselia", "White Crow"})
            self.assertEqual(len(skipped), 2)

    def test_duplicate_routes_are_ambiguous(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "manifest.tsv"
            path.write_text("dmm-runtime\t1\nfile\tcharacter/body.dds\tone\nfile\tCHARACTER\\body.dds\ttwo\n")
            with self.assertRaisesRegex(ValueError, "Conflicting mounted routes"):
                setup.routes_from(path)


if __name__ == "__main__":
    unittest.main()

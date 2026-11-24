from pathlib import PurePosixPath

EXCLUDED_NAMES={'setup dancer.cmd','prepare_dancer_dependencies.py','test_dancer_dependencies.py','dancer_palette.cpp','dancer_palette.h','wardrobe_resources.cpp','wardrobe_resources.h','hex-appeal-dancer-bindings.txt','external-sources.txt','dancer-setup.txt','cd_phw_00_ub_00_0191.pac','cd_phw_00_lb_00_0191.pac','cd_phw_00_ub_00_0191.pac_xml','cd_phw_00_ub_00_0135_00_02_01_o.dds'}

REQUIRED_RUNTIME_CHECKS = frozenset({
    'sdk_definition_generator', 'skin_setup', 'model', 'auto_apply',
    'frame_dispatch_abi', 'menu', 'input_lifecycle', 'interface_preferences',
    'creation_chain', 'appearance_hook_compatibility', 'feature_compatibility',
    'wardrobe_instance_ownership', 'wardrobe_selection', 'wardrobe_outfit_slots',
    'wardrobe_transport_abi', 'barber_catalog', 'wardrobe_provider_api', 'character_addons',
    'underwear_policy', 'outfit_visibility_ownership', 'witch_skin_routes',
    'served_dependency_routes', 'dependency_discovery', 'native_menu_export',
    'unverified_native_interface', 'public_version_metadata',
    'packaged_runtime_configuration', 'dependency_file_preserved',
})

def manager_acceptance_matches_runtime(acceptance, report, runtime_hash):
    return (
        acceptance.get('verified') is True
        and acceptance.get('final_zip_freshly_imported') is True
        and report.get('candidate_installed_files_exact_match') is True
        and report.get('public_runtime_sha256') == runtime_hash
    )

def validate_release_path(name):
    if PurePosixPath(name).name.casefold() in EXCLUDED_NAMES:
        raise ValueError('Excluded outfit integration file: '+name)

def validate_runtime(data):
    for marker in ('Sexy Witches','cd_phw_00_ub_00_0191_u','Dancer colour','HEX-APPEAL-DANCER-BINDINGS'):
        if any(marker.encode(encoding) in data for encoding in ('utf-8','utf-16le')):
            raise ValueError('The runtime still contains the excluded outfit integration: '+marker)

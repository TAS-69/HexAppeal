from pathlib import PurePosixPath

EXCLUDED_NAMES={'setup dancer.cmd','prepare_dancer_dependencies.py','test_dancer_dependencies.py','dancer_palette.cpp','dancer_palette.h','wardrobe_resources.cpp','wardrobe_resources.h','hex-appeal-dancer-bindings.txt','external-sources.txt','dancer-setup.txt','cd_phw_00_ub_00_0191.pac','cd_phw_00_lb_00_0191.pac','cd_phw_00_ub_00_0191.pac_xml','cd_phw_00_ub_00_0135_00_02_01_o.dds'}

def validate_release_path(name):
    if PurePosixPath(name).name.casefold() in EXCLUDED_NAMES:
        raise ValueError('Excluded outfit integration file: '+name)

def validate_runtime(data):
    for marker in ('Sexy Witches','cd_phw_00_ub_00_0191_u','Dancer colour','HEX-APPEAL-DANCER-BINDINGS'):
        if any(marker.encode(encoding) in data for encoding in ('utf-8','utf-16le')):
            raise ValueError('The runtime still contains the excluded outfit integration: '+marker)

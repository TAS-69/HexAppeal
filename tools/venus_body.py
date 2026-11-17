from pathlib import Path

BODY_PATH=Path('files/0009/character/model/1_pc/2_phw/nude/phw_00_nude_00_0001_damian.pac')

def reference_hashes(metadata):
    return {metadata['sha256'],*(body['sha256'] for body in metadata.get('verified_variants',[]))}

def refit_reference(metadata,digest):
    for body in metadata.get('verified_variants',[]):
        if body['sha256']==digest:return body['refit_reference_sha256']
    if digest==metadata['sha256']:return digest
    raise ValueError('This VENUS body has not been checked against the default-outfit refit baseline.')

def active_body(config,config_path):
    selected=[]
    for name in config.get('activeBrowserMods',[]):
        normalized=name.replace('\\','/')
        if normalized.casefold().startswith('venus') and normalized.rsplit('/',1)[-1].casefold() in {'[01] human body','[02] elf body'}:selected.append(normalized)
    if len(selected)!=1:raise ValueError('Enable exactly one VENUS Human Body or Elf Body variant in DMM.')
    storage=Path(config.get('modsPath') or 'mods')
    if not storage.is_absolute():storage=config_path.parent/storage
    storage=storage.resolve();folder=(storage/selected[0]).resolve()
    if not folder.is_relative_to(storage):raise ValueError('The active VENUS folder is outside the configured mod storage.')
    return selected[0],folder/BODY_PATH

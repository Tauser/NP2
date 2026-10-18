"""Preserve current corrected V06, create V07, verify and export print parts."""
import importlib.util
def run(_context: str):
    for filename in ['new-v07.py','check-access-v07.py','open-connectors-v07.py','internal-io-v07.py','verify-v07.py','check-access-v07.py','finish-v07.py','render-access-v07.py','export-print-v07.py']:
        path='D:/Projetos/NP2/tools/cad/'+filename
        spec=importlib.util.spec_from_file_location(filename,path)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        module.run(_context)
    print('Run tools/cad/package-print-v07.py with the bundled Python runtime to check meshes and package the ZIP.')

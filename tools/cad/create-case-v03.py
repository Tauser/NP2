"""One-shot regeneration of final concept in a new Fusion document."""
import importlib.util

def run(_context: str):
    for filename in ['build-v03.py','level-front.py','finish-export.py']:
        spec=importlib.util.spec_from_file_location(filename,'D:/Projetos/NP2/tools/cad/'+filename)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        module.run(_context)

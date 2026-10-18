"""Create V06 from the saved V05, revise and validate, then export print parts."""
import importlib.util
def run(_context: str):
    for filename in ['new-v06.py','simplify-v06.py','flush-bezel-v06.py','inspect-bezel-seam.py','verify-v06.py','finish-v06.py','export-print-v06.py']:
        spec=importlib.util.spec_from_file_location(filename,'D:/Projetos/NP2/tools/cad/'+filename)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        module.run(_context)
    print('Run tools/cad/package-print-v06.py with bundled Python to validate meshes and package the print ZIP.')

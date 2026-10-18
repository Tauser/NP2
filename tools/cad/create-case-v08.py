import importlib.util
def run(_context: str):
    for filename in ['new-v08.py','lower-floor-v08.py','repair-access-v08.py','verify-v08.py','check-access-v08.py','finish-v08.py','render-access-v08.py','export-print-v08.py']:
        path='D:/Projetos/NP2/tools/cad/'+filename
        spec=importlib.util.spec_from_file_location(filename,path)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.run(_context)
    print('Run tools/cad/package-print-v08.py with bundled Python to validate STL/3MF and create ZIP')

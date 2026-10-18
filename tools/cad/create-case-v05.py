"""Reproduce the final revision in a new document; preserve older documents."""
import importlib.util

def run(_context: str):
    for filename in ['build-v05.py','fit-controls-v05.py','recess-usb-v05.py','integrate-speaker-v05.py','level-front.py','align-power-v05.py','verify-temp-v05.py','finish-v05.py']:
        spec=importlib.util.spec_from_file_location(filename,'D:/Projetos/NP2/tools/cad/'+filename)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        module.run(_context)

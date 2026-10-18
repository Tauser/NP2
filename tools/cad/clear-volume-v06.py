import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V06' in app.activeDocument.name
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for x in [-12,6]:v.c.top(shell,52.3,'Folga da flange volume / manter curso',x,-18,12,10,4.9,2.8,v.OP.CutFeatureOperation)
    print('VOLUME flange clearances 0.3 mm; pocket allows 0.5 mm reference travel')

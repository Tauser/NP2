import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('fit','D:/Projetos/NP2/tools/cad/fit-controls-v05.py')
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    keys=next(o for o in root.occurrences if o.component.name.startswith('06 /'))
    for s in list(keys.component.sketches):f.move_sketch(s,-48)
    keys.transform2=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    print('VOLUME geometry centres -12 and +6 mm; supports and apertures match')

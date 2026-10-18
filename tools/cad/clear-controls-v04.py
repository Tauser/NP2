import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    s=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for label,y,w,h,z in [('USB UART',-34.93,11,6,-10)]:
        v.c.side(s,v.RIGHT+.2,'Acabamento final / '+label,y,z,w,h,1.4,-4,v.OP.CutFeatureOperation)
    v.c.side(s,v.RIGHT+.2,'Acabamento final / janela ON OFF',37.82,-9,16,7.2,2.4,-4,v.OP.CutFeatureOperation)
    v.c.side(s,80.8,'Bolso INTERNO cursor / retencao e folga',34.5,-13,14.4,10,2,1.8,v.OP.CutFeatureOperation)
    s.isConstructionFolderLightBulbOn=False
    for p in s.sketches:p.isLightBulbOn=False
    print('CONTROLS CLEARED / external ports preserved')

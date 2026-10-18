import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('s','D:/Projetos/NP2/tools/cad/simplify-v06.py')
s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V06' in app.activeDocument.name
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    s.m.slab(shell,'Uniao positiva painel USB e apoio inferior / evitar aresta tangente',1.2,2.4,.3,-82.4,-44,-15.45,-2.2)
    s.merge(shell,'Eliminar contato de espessura zero no apoio')
    print('USB panel and lower-left PCB support joined with positive wall thickness')

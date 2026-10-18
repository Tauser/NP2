import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V04' in app.activeDocument.name
    old=next(o for o in root.occurrences if o.component.name.startswith('08 /'))
    transform=old.transform2;old.deleteMe()
    o,p=m.component(root,'08 / Cursor lateral ON OFF / aciona chave da placa',transform)
    a=d.appearances.itemByName('NP2 teclas')
    f=c.side(p,v.RIGHT-.1,'Tecla externa ON OFF',37.82,-9,10,6.6,2.2,1.7,OP.NewBodyFeatureOperation)
    c.side(p,83,'Ponte do cursor ate a chave',37.82,-9,5,4,.5,v.RIGHT-82.8,OP.JoinFeatureOperation)
    c.side(p,81.3,'Flange de retencao / abaixo da placa e do apoio',34.5,-13,10,9,2,.9,OP.JoinFeatureOperation)
    c.side(p,82,'Garfo chave / medir lingueta',37.82,-9,5,4,.5,2.2,OP.JoinFeatureOperation)
    c.side(p,81.9,'Rasgo para lingueta original',37.82,-9,2,2,.3,2.3,OP.CutFeatureOperation)
    for b in p.bRepBodies:b.appearance=a
    o.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    p.isOriginFolderLightBulbOn=False;p.isConstructionFolderLightBulbOn=False
    for s in p.sketches:s.isLightBulbOn=False
    print('POWER REPLACED')

import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP
def move_sketch(s,dx,positive_only=False):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    s.timelineObject.rollTo(False)
    s.isComputeDeferred=True;objects=adsk.core.ObjectCollection.create()
    for curve in s.sketchCurves:
        if positive_only and curve.geometry.center.x<0:continue
        curve.isFixed=False;objects.add(curve)
    t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(dx/10,0,0)
    assert s.move(objects,t)
    for curve in objects:curve.isFixed=True
    s.isComputeDeferred=False;s.isLightBulbOn=False
    d.timeline.moveToEnd()
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    front=next(o.component for o in root.occurrences if o.component.name.startswith('01 /'))
    prefixes=['Janela tecla volume / perfil','Suporte tact switches / integrado no corpo / perfil','Pilar suporte volume / ligado ao teto / perfil','Nicho tact switch / perfil']
    moved=[]
    for s in list(shell.sketches):
        if any(s.name.startswith(p) for p in prefixes):move_sketch(s,-48);moved.append(s.name)
    keys=next(o for o in root.occurrences if o.component.name.startswith('06 /'))
    for s in list(keys.component.sketches):move_sketch(s,-48)
    keys.transform2=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2
    move_sketch(next(s for s in front.sketches if s.name.startswith('MIC 1 e MIC 2')),8,True)
    move_sketch(shell.sketches.itemByName('Bocal MIC (1)'),8)
    m.cut_round(front,'Alivio OCULTO para flex touch / manter pele frontal 0.7',19.2,2,.4,63.99,-50.25,-.7,-2.5)
    old=next(o for o in root.occurrences if o.component.name.startswith('08 /'))
    transform=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2.copy();old.deleteMe()
    o,p=m.component(root,'08 / Cursor ON OFF / lingueta do modelo oficial',transform)
    y,z=32.7931,-10.45
    c.side(p,v.LEFT+.1,'Tecla externa ON OFF',y,z,8,4.6,1.8,-1.7,OP.NewBodyFeatureOperation)
    c.side(p,v.LEFT+.2,'Ponte ate lingueta',y,z,4,3,.5,5.2,OP.JoinFeatureOperation)
    c.side(p,-86.2,'Flange interna / retencao',y,z-3,14,7,2,.9,OP.JoinFeatureOperation)
    c.side(p,-84.1,'Garfo para lingueta',y,z,4,3,.5,1,OP.JoinFeatureOperation)
    c.side(p,-84.25,'Folga lingueta / envelope STEP + 0.2 mm',y,z,1.7,1.1,.2,1.35,OP.CutFeatureOperation)
    for b in p.bRepBodies:b.appearance=d.appearances.itemByName('NP2 teclas')
    o.transform2=transform
    for s in p.sketches:s.isLightBulbOn=False
    p.isOriginFolderLightBulbOn=False;p.isConstructionFolderLightBulbOn=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    print('MOVED',moved,'POWER BODIES',p.bRepBodies.count)
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            print('HEALTH',it.name,it.objectType,it.errorOrWarningMessage)

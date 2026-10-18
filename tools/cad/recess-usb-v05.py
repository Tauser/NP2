"""Revision of the active V05: recessed USB panel and fitted power slider."""
import adsk.core, adsk.fusion, importlib.util
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    assert not shell.features.extrudeFeatures.itemByName('Painel USB rebaixado / parede 1.2 mm')
    c.side(shell,v.LEFT+.05,'Painel USB rebaixado / parede 1.2 mm',-16.2825,-12.9,58.765,17.5,2,-82.5-(v.LEFT+.05),OP.JoinFeatureOperation)
    c.side(shell,v.LEFT-.25,'Rebaixo externo / acesso capa dos plugs',-16.2825,-12.9,54.765,13.5,2,-83.7-(v.LEFT-.25),OP.CutFeatureOperation)
    for label,y,w,h,z in [('USB A',-.4,19,11,-12.60),('USB C',-19.1501,13.5,8.5,-10.6195),('USB UART',-34.415,13.5,8.5,-10.6195)]:
        c.side(shell,v.LEFT-.25,'Boca rebaixada / '+label,y,z,w,h,1.7,7,OP.CutFeatureOperation)
    main=shell.bRepBodies.item(0)
    fs=m.sketch(shell,-3.03,'Acabamento exterior do painel rebaixado')
    m.rounded(fs,v.W-.06,108.94,5.97,v.CX,.5);m.finish(fs)
    rs=v.rear_sketch(shell,'Traseira de acabamento do painel',.03)
    v.rear_outline(rs,v.REAR_W-.06,v.REAR_H-.06,5.97,v.CX,0);m.finish(rs)
    inp=shell.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    inp.loftSections.add(fs.profiles.item(0));inp.loftSections.add(rs.profiles.item(0))
    outer=shell.features.loftFeatures.add(inp)
    tools=adsk.core.ObjectCollection.create();tools.add(outer.bodies.item(0))
    ci=shell.features.combineFeatures.createInput(main,tools);ci.operation=OP.IntersectFeatureOperation
    shell.features.combineFeatures.add(ci).name='Painel rebaixado limitado ao corpo arredondado'
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
    c.side(shell,v.LEFT-.2,'Guia ON OFF / folga vertical ajustada',33.9178,z,12,5.6,2,8,OP.CutFeatureOperation)
    for occ in root.occurrences:
        occ.component.isOriginFolderLightBulbOn=False;occ.component.isConstructionFolderLightBulbOn=False
        for s in occ.component.sketches:s.isLightBulbOn=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    print('USB PANEL outer X -83.7 mm; inner X -82.5 mm; A recess 1.7 mm; C recess 2.0182 mm')
    print('SHELL BODIES',shell.bRepBodies.count,'POWER BODIES',p.bRepBodies.count)
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:print('HEALTH',it.name,it.objectType,it.errorOrWarningMessage)

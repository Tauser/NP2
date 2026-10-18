import adsk.core, adsk.fusion, importlib.util, math
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;OP=v.OP
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V04' in app.activeDocument.name
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    for f in list(shell.features.combineFeatures):
        if f.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:f.deleteMe()
    shell.features.loftFeatures.itemByName('Envelope de acabamento do exterior R6').deleteMe()
    main=shell.bRepBodies.item(0)
    sf=m.sketch(shell,-2.9,'Envelope externo / tolerancia booleana 0.1 mm')
    m.rounded(sf,v.W+.2,109.2,6.1,v.CX,.5);m.finish(sf)
    sr=v.rear_sketch(shell,'Envelope traseiro / tolerancia booleana 0.1 mm',-.1)
    v.rear_outline(sr,v.REAR_W+.2,v.REAR_H+.2,6.1,v.CX,0);m.finish(sr)
    li=shell.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    li.loftSections.add(sf.profiles.item(0));li.loftSections.add(sr.profiles.item(0))
    outer=shell.features.loftFeatures.add(li);outer.name='Envelope de acabamento / folga numerica 0.1 mm'
    tools=adsk.core.ObjectCollection.create();tools.add(outer.bodies.item(0))
    ci=shell.features.combineFeatures.createInput(main,tools);ci.operation=OP.IntersectFeatureOperation
    cf=shell.features.combineFeatures.add(ci);cf.name='Limitar apoios ao exterior arredondado'
    t=adsk.core.Matrix3D.create()
    t.setToRotation(math.radians(90-v.TILT),adsk.core.Vector3D.create(1,0,0),m.pt(0,0,0))
    t.translation=adsk.core.Vector3D.create(0,v.H/20*math.sin(math.radians(v.TILT)),v.H/20*math.cos(math.radians(v.TILT)))
    for o in root.occurrences:
        o.isGroundToParent=False;o.isGrounded=False;o.transform2=t
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    root.isOriginFolderLightBulbOn=False;root.isConstructionFolderLightBulbOn=False
    print('REPAIRED',shell.bRepBodies.count,'shell bodies')

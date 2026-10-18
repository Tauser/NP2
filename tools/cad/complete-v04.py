import adsk.core, adsk.fusion, importlib.util, math
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;V=v.V;OP=v.OP
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V04' in app.activeDocument.name
    shell=next(o.component for o in root.occurrences if o.component.name.startswith('02 /'))
    s=v.rear_sketch(shell,'4 torres da tampa / integradas nas paredes',0)
    for xx in [-78,78]:
        for yy in [-v.REAR_H/2+10,v.REAR_H/2-10]:
            v.rear_outline(s,12 if xx<0 else 10,9,2,-81 if xx<0 else 79,yy)
    m.extrude(shell,s,8,OP.NewBodyFeatureOperation,'Torres de fixacao tampa / M3')
    s=v.rear_sketch(shell,'4 pilotos das torres da tampa',-.1)
    center=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
    for xx in [-78,78]:
        for yy in [-v.REAR_H/2+10,v.REAR_H/2-10]:
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(center.x*10+xx,center.y*10+yy),.13)
    m.extrude(shell,s,8.2,OP.CutFeatureOperation,'Pilotos M3 tampa / ajustar conforme parafuso')
    main=shell.bRepBodies.item(0);tools=adsk.core.ObjectCollection.create()
    for b in shell.bRepBodies:
        if b!=main:tools.add(b)
    ci=shell.features.combineFeatures.createInput(main,tools);ci.operation=OP.JoinFeatureOperation
    joined=shell.features.combineFeatures.add(ci);joined.name='Torres integradas ao corpo';main=joined.bodies.item(0)
    fs=m.sketch(shell,-2.9,'Envelope externo / tolerancia booleana 0.1 mm')
    m.rounded(fs,v.W+.2,109.2,6.1,v.CX,.5);m.finish(fs)
    rs=v.rear_sketch(shell,'Envelope traseiro / tolerancia booleana 0.1 mm',-.1)
    v.rear_outline(rs,v.REAR_W+.2,v.REAR_H+.2,6.1,v.CX,0);m.finish(rs)
    li=shell.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    li.loftSections.add(fs.profiles.item(0));li.loftSections.add(rs.profiles.item(0))
    outer=shell.features.loftFeatures.add(li);outer.name='Envelope de acabamento / folga numerica 0.1 mm'
    tools=adsk.core.ObjectCollection.create();tools.add(outer.bodies.item(0))
    ci=shell.features.combineFeatures.createInput(main,tools);ci.operation=OP.IntersectFeatureOperation
    cf=shell.features.combineFeatures.add(ci);cf.name='Limitar apoios ao exterior do corpo'
    t=adsk.core.Matrix3D.create()
    t.setToRotation(math.radians(90-v.TILT),adsk.core.Vector3D.create(1,0,0),m.pt(0,0,0))
    t.translation=adsk.core.Vector3D.create(0,v.H/20*math.sin(math.radians(v.TILT)),v.H/20*math.cos(math.radians(v.TILT)))
    for o in root.occurrences:
        o.isGroundToParent=False;o.isGrounded=False;o.transform2=t
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    root.isOriginFolderLightBulbOn=False;root.isConstructionFolderLightBulbOn=False
    print('FINISHED',root.occurrences.count,'components',d.timeline.count,'features','shell bodies',shell.bRepBodies.count)

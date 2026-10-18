import adsk.core, adsk.fusion, os, json

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V03' in app.activeDocument.name
    root=d.rootComponent
    issues=[]
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:issues.append((it.name,it.errorOrWarningMessage))
    print('HEALTH',json.dumps(issues))
    pts=[]
    for o in root.occurrences:
        for b in o.component.bRepBodies:
            for vertex in b.vertices:
                p=vertex.geometry.copy();p.transformBy(o.transform2);pts.append(p)
    print('WORLD BOUNDS MM',[(max(getattr(p,ax) for p in pts)-min(getattr(p,ax) for p in pts))*10 for ax in ['x','y','z']])
    print('LOWEST MM',min(p.z for p in pts)*10)
    print('SELECTCMD',bool(app.userInterface.commandDefinitions.itemById('SelectCommand')))
    select=app.userInterface.commandDefinitions.itemById('SelectCommand')
    if select:select.execute()
    # Grid controls are toggle commands; read checkbox state before turning them off.
    for cid in ['ViewLayoutGridOnCommand','SketchGridOn']:
        cmd=app.userInterface.commandDefinitions.itemById(cid)
        definition=adsk.core.CheckBoxControlDefinition.cast(cmd.controlDefinition) if cmd else None
        if definition and definition.isChecked:cmd.execute()
    print('GRID OFF')
    # Set document Front relative to the standing assembly, then present in perspective.
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(0,-35,5.2);cam.target=adsk.core.Point3D.create(0,3.6,5.2)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.setCurrentAsFront()
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(24,-30,20);cam.target=adsk.core.Point3D.create(0,3.6,5.2)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    out='D:/Projetos/NP2/mechanical/concept-v03';os.makedirs(out,exist_ok=True)
    assert app.activeViewport.saveAsImageFile(out+'/np2-front.png',1600,1200)
    cam=app.activeViewport.camera
    print('CAMERA',cam.eye.asArray(),cam.target.asArray(),cam.upVector.asArray())
    print('IMAGE',out+'/np2-front.png')

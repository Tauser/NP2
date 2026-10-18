import adsk.core, adsk.fusion, sys, importlib.util, os, json
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    assert d.activateRootComponent()
    for o in root.occurrences:
        o.isLightBulbOn=True
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    select=app.userInterface.commandDefinitions.itemById('SelectCommand')
    if select:select.execute()
    app.userInterface.activeSelections.clear()
    root.isOriginFolderLightBulbOn=False
    adsk.doEvents()
    for o in root.occurrences:
        o.component.isConstructionFolderLightBulbOn=False;o.component.isOriginFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    issues=[]
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            if it.objectType=='adsk::fusion::TimelineGroup' and not it.errorOrWarningMessage:continue
            issues.append((it.name,it.errorOrWarningMessage))
    print('TIMELINE HEALTH',json.dumps(issues));assert not issues,'Resolve feature errors before export'
    out='D:/Projetos/NP2/mechanical/concept-v05';os.makedirs(out,exist_ok=True)
    app.activeDocument.name='NP2 - Mesa V05 / hardware oficial / corpo R6'
    d.attributes.add('NP2','revision','V05: hardware oficial; USB rebaixado; berco integrado na tampa com quatro travas; corpo R6 e traseira vertical')
    d.attributes.add('NP2','fit_status','CAD verificado contra STEP oficial: zero colisoes. Validar em prototipo cabos, curso ON/OFF, travas e tact switches. Dutos acusticos MIC a definir.')
    export=d.exportManager
    # Rear view for audio layout, then restore upright front view.
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(-22,38,19);cam.target=adsk.core.Point3D.create(0,3,5)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    app.userInterface.activeSelections.clear()
    root.isOriginFolderLightBulbOn=False
    for o in root.occurrences:o.component.isOriginFolderLightBulbOn=False
    assert app.activeViewport.saveAsImageFile(out+'/np2-rear.png',1600,1200)
    # Optional internal view for reviewing speaker placement.
    cap=next(o for o in root.occurrences if o.component.name.startswith('03 /'))
    cap.isLightBulbOn=False
    adsk.doEvents();app.activeViewport.refresh()
    assert app.activeViewport.saveAsImageFile(out+'/np2-inside.png',1600,1200)
    cap.isLightBulbOn=True
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(-35,2.8,5.2);cam.target=adsk.core.Point3D.create(0,2.8,5.2)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.cameraType=adsk.core.CameraTypes.OrthographicCameraType
    cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    assert app.activeViewport.saveAsImageFile(out+'/np2-usb-side.png',1600,1200)
    visibility=[(o,o.isLightBulbOn) for o in root.occurrences]
    for o,_ in visibility:o.isLightBulbOn=(o==cap)
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(18,-25,16);cam.target=adsk.core.Point3D.create(0,5.8,5)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    assert app.activeViewport.saveAsImageFile(out+'/np2-cover-cradle.png',1600,1200)
    for o,on in visibility:o.isLightBulbOn=on
    if select:select.execute()
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(-20,-34,16);cam.target=adsk.core.Point3D.create(0,2.7,5.2)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    app.userInterface.activeSelections.clear()
    root.isOriginFolderLightBulbOn=False
    for o in root.occurrences:o.component.isOriginFolderLightBulbOn=False
    assert app.activeViewport.saveAsImageFile(out+'/np2-front.png',1600,1200)
    # Refresh archive after final saved viewing orientation.
    assert export.execute(export.createFusionArchiveExportOptions(out+'/NP2-case-v05.f3d'))
    assert export.execute(export.createSTEPExportOptions(out+'/NP2-case-v05.step'))
    print('EXPORTED',out,'speaker measured 100 x 45 x 20 mm')

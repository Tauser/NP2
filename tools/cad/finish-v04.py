import adsk.core, adsk.fusion, sys, importlib.util, os, json
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('v04','D:/Projetos/NP2/tools/cad/build-v04.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V04' in app.activeDocument.name
    transform=root.occurrences.item(0).transform2
    for o in root.occurrences:
        o.isLightBulbOn=True
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False
        o.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    select=app.userInterface.commandDefinitions.itemById('SelectCommand')
    if select:select.execute()
    app.userInterface.activeSelections.clear()
    root.isOriginFolderLightBulbOn=False
    adsk.doEvents()
    holder=next(o.component for o in root.occurrences if o.component.name.startswith('05 /'))
    rear=next(o.component for o in root.occurrences if o.component.name.startswith('03 /'))
    if not holder.features.extrudeFeatures.itemByName('4 abas fixacao berco'):
        s=v.rear_sketch(holder,'Abas do berco / fixacao ao painel',0)
        for xx in [-55,55]:
            for yy in [-16,16]:v.rear_outline(s,8,14,2,xx,yy+v.AUDIO_Y)
        m.extrude(holder,s,5,m.OP.JoinFeatureOperation,'4 abas fixacao berco')
        s=v.rear_sketch(holder,'Pilotos das abas',5.1)
        center=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
        for xx in [-56,56]:
            for yy in [-16,16]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(center.x*10+xx,center.y*10+yy+v.AUDIO_Y),.13)
        m.extrude(holder,s,-5.3,m.OP.CutFeatureOperation,'4 pilotos M3 no berco / ajustar inserto')
        s=v.rear_sketch(rear,'Parafusos de fixacao do berco',.1)
        center=s.modelToSketchSpace(m.pt(0,v.MID_Y,v.MID_Z))
        for xx in [-56,56]:
            for yy in [-16,16]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(center.x*10+xx,center.y*10+yy+v.AUDIO_Y),.16)
        m.extrude(rear,s,-2.7,m.OP.CutFeatureOperation,'4 passagens M3 / fixacao berco')
    for o in root.occurrences:
        o.component.isConstructionFolderLightBulbOn=False;o.component.isOriginFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    issues=[]
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:issues.append((it.name,it.errorOrWarningMessage))
    print('TIMELINE HEALTH',json.dumps(issues));assert not issues,'Resolve feature errors before export'
    out='D:/Projetos/NP2/mechanical/concept-v04';os.makedirs(out,exist_ok=True)
    app.activeDocument.name='NP2 - Mesa V04 / traseira reta / corpo R6'
    export=d.exportManager
    assert export.execute(export.createFusionArchiveExportOptions(out+'/NP2-case-v04.f3d'))
    assert export.execute(export.createSTEPExportOptions(out+'/NP2-case-v04.step'))
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
    if select:select.execute()
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(20,-34,16);cam.target=adsk.core.Point3D.create(0,2.7,5.2)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isSmoothTransition=False;cam.isFitView=True
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    app.userInterface.activeSelections.clear()
    root.isOriginFolderLightBulbOn=False
    for o in root.occurrences:o.component.isOriginFolderLightBulbOn=False
    assert app.activeViewport.saveAsImageFile(out+'/np2-front.png',1600,1200)
    # Refresh archive after final saved viewing orientation.
    assert export.execute(export.createFusionArchiveExportOptions(out+'/NP2-case-v04.f3d'))
    print('EXPORTED',out,'speaker measured 100 x 45 x 20 mm')

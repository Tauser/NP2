"""Create a hardware-free print document and oriented millimetre STL files."""
import adsk.core, adsk.fusion, math, os, json

def rotation(axis,angle):
    t=adsk.core.Matrix3D.create();t.setToRotation(angle,adsk.core.Vector3D.create(*axis),adsk.core.Point3D.create(0,0,0));return t

def run(_context: str):
    app=adsk.core.Application.get();assembly=app.activeDocument
    assert 'V06' in assembly.name
    model=adsk.fusion.Design.cast(app.activeProduct);root=model.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
    parts=[]
    for prefix,filename,layout in [('01 /','01-moldura.stl',(202,8)),('02 /','02-corpo.stl',(8,8)),('03 /','03-tampa-berco.stl',(8,130)),('06 /','volume',(220,222)),('08 /','06-cursor-onoff.stl',(276,222))]:
        occ=next(o for o in root.occurrences if o.component.name.startswith(prefix))
        bodies=sorted(list(occ.component.bRepBodies),key=lambda b:b.boundingBox.minPoint.x)
        for idx,body in enumerate(bodies):
            assert body.isSolid and body.lumps.count==1
            if prefix in ['02 /','03 /']:
                temp=mgr.copy(body.createForAssemblyContext(occ));mgr.transform(temp,rotation((1,0,0),-math.pi/2))
            else:
                temp=mgr.copy(body)
                if prefix=='01 /':
                    mgr.transform(temp,rotation((0,1,0),math.pi));mgr.transform(temp,rotation((0,0,1),math.pi/2))
                elif prefix=='06 /':mgr.transform(temp,rotation((1,0,0),-math.pi/2))
                else:mgr.transform(temp,rotation((0,1,0),-math.pi/2))
            bb=temp.boundingBox
            shift=adsk.core.Matrix3D.create();shift.translation=adsk.core.Vector3D.create(-bb.minPoint.x,-bb.minPoint.y,-bb.minPoint.z)
            mgr.transform(temp,shift)
            name=filename;pos=layout
            if prefix=='06 /':name='04-volume-menos.stl' if idx==0 else '05-volume-mais.stl';pos=(220+idx*28,222)
            parts.append((name,temp,pos))
    doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType);doc.name='NP2 - V06 / pecas para ABS / mesa 350x350'
    d=adsk.fusion.Design.cast(app.activeProduct);d.designType=adsk.fusion.DesignTypes.DirectDesignType
    d.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    out='D:/Projetos/NP2/mechanical/concept-v06';os.makedirs(out+'/STL',exist_ok=True)
    manifest=[]
    for name,temp,pos in parts:
        occ=d.rootComponent.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        if hasattr(occ,'isGroundToParent'):occ.isGroundToParent=False
        occ.isGrounded=False;occ.component.name=name[:-4]
        body=occ.component.bRepBodies.add(temp);body.name=name[:-4]
        options=d.exportManager.createSTLExportOptions(body,out+'/STL/'+name)
        options.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        options.unitType=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
        options.isBinaryFormat=True;options.sendToPrintUtility=False
        assert d.exportManager.execute(options)
        bb=body.boundingBox;size=[(a-b)*10 for a,b in zip(bb.maxPoint.asArray(),bb.minPoint.asArray())]
        assert abs(bb.minPoint.z)<1e-7 and pos[0]+size[0]<350 and pos[1]+size[1]<350
        t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(pos[0]/10,pos[1]/10,0);occ.transform2=t
        occ.component.isOriginFolderLightBulbOn=False
        manifest.append({'filename':name,'size_mm':size,'plate_xy_mm':pos,'min_z_mm':bb.minPoint.z*10,'solid':body.isSolid,'connected_lumps':body.lumps.count})
    d.rootComponent.isOriginFolderLightBulbOn=False
    assert d.activateRootComponent()
    bed=d.rootComponent.sketches.add(d.rootComponent.xYConstructionPlane);bed.name='REF / mesa 350 x 350 / nao exportada como malha'
    bed.sketchCurves.sketchLines.addTwoPointRectangle(adsk.core.Point3D.create(0,0,0),adsk.core.Point3D.create(35,35,0))
    for line in bed.sketchCurves:line.isConstruction=True;line.isFixed=True
    bed.isLightBulbOn=True
    cam=adsk.core.Camera.create();cam.viewOrientation=adsk.core.ViewOrientations.ArbitraryViewOrientation
    cam.eye=adsk.core.Point3D.create(45,-40,55);cam.target=adsk.core.Point3D.create(17.5,17.5,1)
    cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isFitView=True;cam.isSmoothTransition=False
    app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
    assert app.activeViewport.saveAsImageFile(out+'/np2-print-layout.png',1600,1200)
    package=d.exportManager.createC3MFExportOptions(d.rootComponent,out+'/NP2-v06-ABS-350x350.3mf')
    package.meshRefinement=adsk.fusion.MeshRefinementSettings.MeshRefinementHigh;package.sendToPrintUtility=False
    assert d.exportManager.execute(package)
    assert d.exportManager.execute(d.exportManager.createFusionArchiveExportOptions(out+'/NP2-v06-pecas-impressao.f3d'))
    with open(out+'/print-manifest.json','w',encoding='utf8') as f:json.dump({'units':'mm','material_requested':'ABS','bed_mm':[350,350],'nozzle_assumed_mm':.4,'parts':manifest},f,ensure_ascii=False,indent=2)
    assembly.activate()
    print('SIX ORIENTED STL FILES',json.dumps(manifest,ensure_ascii=False))

"""Pairwise solid checks against the official hardware; no hardware self-tests."""
import adsk.core, adsk.fusion, json, os, time
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    started=time.monotonic();mgr=adsk.fusion.TemporaryBRepManager.get()
    hw=next(o for o in root.occurrences if o.component.name.startswith('REF OFICIAL'))
    panel=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2
    inverse=panel.copy();inverse.invert()
    def copy_native(body,occ):
        temp=mgr.copy(body.createForAssemblyContext(occ));mgr.transform(temp,inverse);return temp
    def overlaps(a,b):
        return all(getattr(a.maxPoint,k)>getattr(b.minPoint,k)+1e-6 and getattr(b.maxPoint,k)>getattr(a.minPoint,k)+1e-6 for k in ['x','y','z'])
    cases=[]
    for o in root.occurrences:
        if o==hw:continue
        assert all(abs(a-b)<1e-8 for a,b in zip(o.transform2.asArray(),panel.asArray())),o.component.name+' assembly orientation changed'
        for b in o.component.bRepBodies:
            temp=copy_native(b,o);cases.append((o.component.name+'/'+b.name,temp,temp.boundingBox))
    checks=0;clashes=[];failures=[];non_solids=[];body_count=0
    solid_continuity={name:{'solid':body.isSolid,'connected_lumps':body.lumps.count} for name,body,_ in cases}
    def check(one,a,two,b):
        nonlocal checks
        checks+=1;copy=mgr.copy(a)
        ok=mgr.booleanOperation(copy,b,adsk.fusion.BooleanTypes.IntersectionBooleanType)
        if not ok:failures.append([one,two]);return
        if copy.volume*1000>.01:
            bb=copy.boundingBox
            clashes.append({'one':one,'two':two,'volume_mm3':copy.volume*1000,'local_bbox_mm':[[x*10 for x in bb.minPoint.asArray()],[x*10 for x in bb.maxPoint.asArray()]]})
    for o in root.allOccurrences:
        if not o.fullPathName.startswith(hw.fullPathName+'+'):continue
        for body in o.component.bRepBodies:
            body_count+=1;t=copy_native(body,o);bb=t.boundingBox
            if not t.isSolid:
                non_solids.append(o.fullPathName+'/'+body.name)
                center=adsk.core.Point3D.create(*[(getattr(bb.minPoint,k)+getattr(bb.maxPoint,k))/2 for k in ['x','y','z']])
                bounds=adsk.core.OrientedBoundingBox3D.create(center,adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),*[max(getattr(bb.maxPoint,k)-getattr(bb.minPoint,k),.005) for k in ['x','y','z']])
                t=mgr.createBox(bounds);bb=t.boundingBox
            # Central hardware is inside an empty, verified part of the hollow shell.
            central=bb.minPoint.x>-7.3 and bb.maxPoint.x<7.3 and bb.minPoint.y>-3.8 and bb.maxPoint.y<3.8 and bb.minPoint.z>-1.9 and bb.maxPoint.z<-.6
            for name,c,cb in cases:
                if name.startswith('02 /') and central:continue
                if overlaps(cb,bb):check(name,c,o.fullPathName+'/'+body.name,t)
    for i,(name,a,bb) in enumerate(cases):
        for name2,b,bb2 in cases[i+1:]:
            if overlaps(bb,bb2):check(name,a,name2,b)
    health=[];import_groups=[]
    for i in range(d.timeline.count):
        it=d.timeline.item(i)
        if it.healthState==adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:continue
        if it.objectType=='adsk::fusion::TimelineGroup' and not it.errorOrWarningMessage:import_groups.append(it.name)
        else:health.append((it.name,it.errorOrWarningMessage))
    bounds={}
    for o in root.occurrences:
        if o==hw:continue
        coords=[]
        for b in o.component.bRepBodies:
            bb=mgr.copy(b.createForAssemblyContext(o)).boundingBox
            coords.extend([bb.minPoint.asArray(),bb.maxPoint.asArray()])
        bounds[o.component.name]=[[min(p[k] for p in coords)*10 for k in range(3)],[max(p[k] for p in coords)*10 for k in range(3)]]
    data={'source':'Waveshare STEP 20251108','hardware_body_count':body_count,'boolean_checks':checks,'clashes':clashes,'boolean_failures':failures,'non_solid_hardware_checked_as_conservative_boxes':non_solids,'timeline_issues':health,'empty_import_group_warnings':import_groups,'solid_continuity':solid_continuity,'component_world_bounds_mm':bounds,'elapsed_seconds':time.monotonic()-started}
    out='D:/Projetos/NP2/mechanical/concept-v05';os.makedirs(out,exist_ok=True)
    with open(out+'/fit-check.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
    print('CHECKS',checks,'HARDWARE',body_count,'CLASHES',len(clashes),'FAILURES',len(failures),'NON SOLIDS',len(non_solids),'SECONDS',data['elapsed_seconds'])
    for c in clashes[:30]:print(json.dumps(c,ensure_ascii=False))
    print('HEALTH',health)
    print('CONTINUITY',json.dumps(solid_continuity,ensure_ascii=False))
    assert body_count==1345,'Official assembly body count changed'
    assert not clashes and not failures and not health,'Resolve fit issues before exporting'
    assert all(body['solid'] and body['connected_lumps']==1 for body in solid_continuity.values()),'Printable pieces must be connected solids'

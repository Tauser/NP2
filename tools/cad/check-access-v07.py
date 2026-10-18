"""Check insertion corridors against the shell in panel coordinates.

USB overmolds are reference envelopes, not measured user cable models.
For lower connectors, use horizontal-header mating envelopes and a
microSD card corridor extending in the actual -Y insertion direction.
"""
import adsk.core, adsk.fusion, json, os
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    root=d.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
    shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
    t=shell.transform2;inverse=t.copy();inverse.invert()
    parts=[]
    for o in root.occurrences:
        if o.component.name.startswith('REF'):continue
        for b in o.component.bRepBodies:
            temp=mgr.copy(b.createForAssemblyContext(o));mgr.transform(temp,inverse)
            parts.append((o.component.name,temp))
    def box(bounds):
        lo,hi=bounds
        center=adsk.core.Point3D.create(*[(lo[k]+hi[k])/20 for k in range(3)])
        ob=adsk.core.OrientedBoundingBox3D.create(center,adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),*[(hi[k]-lo[k])/10 for k in range(3)])
        return mgr.createBox(ob)
    tests=[
      ('USB A / housing 22 x 13', [[-120,-11.4,-19.1],[-84,10.6,-6.1]]),
      ('USB C / housing 14 x 10', [[-120,-26.1501,-15.6195],[-84.1818,-12.1501,-5.6195]]),
      ('USB UART / housing 14 x 10', [[-120,-41.415,-15.6195],[-84.1818,-27.415,-5.6195]]),
      ('USB A / metal neck', [[-84,-7.15,-15.85],[-82,6.35,-9.35]]),
      ('USB C / metal neck', [[-84.1818,-23.6501,-12.1195],[-81.6818,-14.6501,-9.1195]]),
      ('USB UART / metal neck', [[-84.1818,-38.915,-12.1195],[-81.6818,-29.915,-9.1195]]),
      ('IO right / mating reference 18 x 7', [[36.825,-64,-14.65],[54.825,-46.8,-7.65]]),
      ('IO left / mating reference 18 x 7', [[-40.175,-64,-14.65],[-22.175,-46.8,-7.65]]),
      ('microSD / card and push clearance', [[-8.8,-65,-11.4],[4.5,-34,-9.55]])
    ]
    internal_io=bool(d.attributes.itemByName('NP2','internal_io_v07'))
    result=[]
    for label,bounds in tests:
        corridor=box(bounds);clashes=[]
        for name,part in parts:
            temp=mgr.copy(corridor)
            assert mgr.booleanOperation(temp,part,adsk.fusion.BooleanTypes.IntersectionBooleanType)
            if temp.volume*1000>.01:clashes.append({'part':name,'volume_mm3':temp.volume*1000})
        vertices=[]
        for x in [bounds[0][0],bounds[1][0]]:
            for y in [bounds[0][1],bounds[1][1]]:
                for z in [bounds[0][2],bounds[1][2]]:
                    p=adsk.core.Point3D.create(x/10,y/10,z/10);p.transformBy(t);vertices.append(p)
        pending_internal=internal_io and label.startswith('IO ')
        result.append({'name':label,'reference_bounds_mm':bounds,'case_intersections':clashes,'world_min_z_mm':min(p.z*10 for p in vertices),'validation_status':'pending real internal mating cable dimensions' if pending_internal else 'required service access'})
    corrected=bool(d.attributes.itemByName('NP2','connector_access_v07'))
    stage='after' if corrected else 'before'
    out='D:/Projetos/NP2/mechanical/concept-v07'
    with open(out+'/connector-access-'+stage+'.json','w',encoding='utf8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
    print(json.dumps(result,ensure_ascii=False))
    if corrected:assert not any(r['case_intersections'] for r in result if r['validation_status']=='required service access'),'Resolve insertion corridor obstructions'
    if d.attributes.itemByName('NP2','table_clearance_v07'):
        assert all(r['world_min_z_mm']>1.8 for r in result),'Table obstructs insertion path'

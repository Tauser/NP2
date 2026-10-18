"""Measure the bezel excess against the body's front section without editing CAD."""
import adsk.core, adsk.fusion, math, json, os

def run(_context: str):
    app=adsk.core.Application.get(); d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V06' in app.activeDocument.name
    mgr=adsk.fusion.TemporaryBRepManager.get()
    front=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('01 /'))
    body=front.component.bRepBodies.item(0)
    def box(x,y,z,w,h,t):
        ob=adsk.core.OrientedBoundingBox3D.create(adsk.core.Point3D.create(x/10,y/10,z/10),adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),w/10,h/10,t/10)
        return mgr.createBox(ob)
    w,h,r,cx,cy=174.22,108.94,5.97,-1.8,.5
    envelope=box(cx,cy,-1.5,w-2*r,h,3.2)
    assert mgr.booleanOperation(envelope,box(cx,cy,-1.5,w,h-2*r,3.2),adsk.fusion.BooleanTypes.UnionBooleanType)
    for x in [cx-w/2+r,cx+w/2-r]:
        for y in [cy-h/2+r,cy+h/2-r]:
            cyl=mgr.createCylinderOrCone(adsk.core.Point3D.create(x/10,y/10,-.31),r/10,adsk.core.Point3D.create(x/10,y/10,.01),r/10)
            assert mgr.booleanOperation(envelope,cyl,adsk.fusion.BooleanTypes.UnionBooleanType)
    excess=mgr.copy(body)
    assert mgr.booleanOperation(excess,envelope,adsk.fusion.BooleanTypes.DifferenceBooleanType)
    out={}
    for label,x,y in [('lower_left',-85,-51),('lower_right',82,-51),('upper_left',-85,52),('upper_right',82,52)]:
        piece=mgr.copy(excess)
        assert mgr.booleanOperation(piece,box(x,y,-1.5,15,15,3.2),adsk.fusion.BooleanTypes.IntersectionBooleanType)
        out[label]={'excess_mm3':piece.volume*1000}
        if piece.volume>1e-9:
            bb=piece.boundingBox
            out[label]['bounds_mm']=[[v*10 for v in bb.minPoint.asArray()],[v*10 for v in bb.maxPoint.asArray()]]
    stage='after' if d.attributes.itemByName('NP2','flush_bezel_v06') else 'before'
    path='D:/Projetos/NP2/mechanical/concept-v06/bezel-seam-'+stage+'.json'
    with open(path,'w') as f:json.dump(out,f,indent=2)
    print(json.dumps(out))
    print('FRONT OUTER PLANAR EDGES')
    for e in body.edges:
        bb=e.boundingBox
        if abs(bb.minPoint.z+.3)<1e-6 and abs(bb.maxPoint.z+.3)<1e-6:
            print(e.geometry.objectType,[[v*10 for v in bb.minPoint.asArray()],[v*10 for v in bb.maxPoint.asArray()]])

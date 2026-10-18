"""Finish the first outline: its vertical endpoints were geometrically
coincident, but not constrained to the lower arcs in the imported model."""
import adsk.core, adsk.fusion, math
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    comp=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    s=comp.sketches.itemByName('Contorno frontal do corpo / R6')
    s.timelineObject.rollTo(False);s.isComputeDeferred=True
    selected=adsk.core.ObjectCollection.create()
    for curve in s.sketchCurves:
        curve.isFixed=False
        g=curve.geometry
        if g.objectType=='adsk::core::Line3D' and abs(g.startPoint.x-g.endPoint.x)<1e-7:
            p=curve.startSketchPoint if g.startPoint.y<g.endPoint.y else curve.endSketchPoint
            assert abs(p.geometry.y+4.8)<1e-6
            selected.add(p)
    t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(0,-.8/math.cos(math.radians(18)),0)
    assert selected.count==2 and s.move(selected,t)
    for curve in s.sketchCurves:curve.isFixed=True
    s.isComputeDeferred=False;assert s.profiles.count==1
    s.attributes.add('NP2','lowered_v08',str(8/math.cos(math.radians(18))));s.isLightBulbOn=False
    d.timeline.moveToEnd();print('First outline repaired; closed profile and stretched vertical sides')

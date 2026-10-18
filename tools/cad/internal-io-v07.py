"""User selected optional internal IO, rather than permanent external cables."""
import adsk.core, adsk.fusion, json
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    comp=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    attr=d.attributes.itemByName('NP2','table_clearance_v07')
    if attr:
        for label in ['direito','esquerdo']:
            f=comp.features.extrudeFeatures.itemByName('Apoio '+label+' integrado / 15 mm livres sob corpo')
            assert f and f.deleteMe()
        for o in d.rootComponent.occurrences:
            t=o.transform2.copy();p=t.translation;p.z-=1.5;t.translation=p;o.transform2=t
        if d.snapshots.hasPendingSnapshot:d.snapshots.add()
        assert attr.deleteMe()
    # Keep the lower wall intact for internal-only IO. Real mating plugs
    # and any add-on boards must be chosen before sizing internal pockets.
    for label in ['IO esquerdo','IO direito']:
        f=comp.features.extrudeFeatures.itemByName('Acesso inferior / '+label)
        if f:assert f.deleteMe()
    d.timeline.moveToEnd()
    for s in comp.sketches:s.isLightBulbOn=False
    assert comp.bRepBodies.count==1 and comp.bRepBodies.item(0).lumps.count==1
    d.attributes.add('NP2','internal_io_v07',json.dumps({'selection':'optional internal components','external_io_openings':False,'extra_feet':False,'mating_cable_dimensions_pending':True,'microsd_service':'lift unit to insert/remove through bottom opening'}))
    print('Internal-only IO selected: 15 mm feet and external IO windows removed; microSD and enlarged USB bay retained')

"""Rebuild USB back wall with complete borders; replace oversized SD cut."""
import adsk.core, adsk.fusion, importlib.util, json
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V08' in app.activeDocument.name
    assert d.attributes.itemByName('NP2','lower_floor_v08')
    if d.attributes.itemByName('NP2','access_repair_v08'):
        print('Access repair already applied');return
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    assert shell.bRepBodies.count==1 and shell.bRepBodies.item(0).lumps.count==1,'Repair upstream unions before replacing accesses'
    for name in ['microSD / abertura e acesso para pressionar','USB / baia externa ampla para capas dos plugs']:
        f=shell.features.extrudeFeatures.itemByName(name);assert f and f.deleteMe(),name
    d.timeline.moveToEnd()
    c.side(shell,-83.7,'USB / parede continua 1.4 mm / bases completas',-15.7,-13.6,62,21,1.5,1.4,OP.JoinFeatureOperation)
    for label,y,w,h,z in [('USB A',-.4,19,11,-12.60),('USB C',-19.1501,13.5,8.5,-10.6195),('USB UART',-34.415,13.5,8.5,-10.6195)]:
        c.side(shell,-89.3,'USB / boca alinhada / '+label,y,z,w,h,1.2,7.7,OP.CutFeatureOperation)
    c.side(shell,-89.3,'USB / rebaixo 58 x 16.2 / bordas completas',-15.7,-12.4,58,16.2,1,5.6,OP.CutFeatureOperation)
    c.top(shell,-44,'microSD / fenda estreita 16 x 4',-2.1301,-10.5,16,4,.6,38,OP.CutFeatureOperation)
    m.circles(shell,'MIC / canais livres nos apoios novos',[(x,-51,2) for x in [-68,76]],-3.02,-3.1,OP.CutFeatureOperation)
    assert shell.bRepBodies.count==1 and shell.bRepBodies.item(0).isSolid and shell.bRepBodies.item(0).lumps.count==1
    for s in shell.sketches:s.isLightBulbOn=False
    shell.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','access_repair_v08',json.dumps({'sd_slot_mm':[16,4],'old_sd_slot_mm':[22,12],'usb_wall_mm':1.4,'usb_recess_mm':[58,16.2],'extra_floor_clearance_mm':8,'internal_reference_plugs_mm':[32,7,6.2],'real_plugs_not_measured':True}))
    print('SD slot reduced from 22 x 12 to 16 x 4 mm; USB back wall rebuilt and individual mouths reopened; MIC ducts clear')

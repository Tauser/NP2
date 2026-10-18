"""Open the lower IO/microSD paths and enlarge the shared USB recess."""
import adsk.core, adsk.fusion, importlib.util, json
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;c=v.c;OP=v.OP
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V07' in app.activeDocument.name
    if d.attributes.itemByName('NP2','connector_access_v07'):
        print('Connector openings already applied');return
    shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    # Outer boot recess is separate from the already-open metal neck holes.
    # All three mouths share one recess; there are no narrow exterior webs
    # obstructing large molded cable housings between adjacent USB ports.
    c.side(shell,-89.3,'USB / baia externa ampla para capas dos plugs',-15.7,-12.9,58,18,1.5,5.6,OP.CutFeatureOperation)
    for x,label in [(45.825,'IO direito'),(-31.175,'IO esquerdo')]:
        c.top(shell,-42,'Acesso inferior / '+label,x,-11.15,22,12,2,32,OP.CutFeatureOperation)
    c.top(shell,-40,'microSD / abertura e acesso para pressionar',-2.1301,-10.5,22,12,2,34,OP.CutFeatureOperation)
    for s in shell.sketches:s.isLightBulbOn=False
    shell.isConstructionFolderLightBulbOn=False
    assert shell.bRepBodies.count==1 and shell.bRepBodies.item(0).lumps.count==1
    d.attributes.add('NP2','connector_access_v07',json.dumps({'usb_reference_housings_mm':{'A':[22,13],'C':[14,10]},'lower_io_openings_mm':[22,12],'microsd_opening_mm':[22,12],'lower_ports_axis':'-Y in panel coordinates','physical_cables_not_measured':True}))
    print('Opened two lower IO bays and microSD push access; USB common recess 58 x 18 mm')

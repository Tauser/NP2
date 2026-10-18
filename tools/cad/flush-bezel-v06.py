"""Make the bezel outline equal to the actual shell front outline.

The old 110 mm bezel outline had lower corner centres 1 mm below the
109 mm shell outline. Levelling its bottom did not trim the corner ears.
Reuse the shell's final 0.03 mm inset profile for all four bezel corners.
Retain the old feature chain through a reversible Remove feature.
"""
import adsk.core, adsk.fusion, importlib.util, json
spec=importlib.util.spec_from_file_location('v05','D:/Projetos/NP2/tools/cad/build-v05.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
m=v.m;OP=v.OP

def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'Mesa V06' in app.activeDocument.name
    front=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('01 /'))
    if d.attributes.itemByName('NP2','flush_bezel_v06'):
        print('Bezel outline correction already applied');return
    assert front.bRepBodies.count==1
    old=front.bRepBodies.item(0)
    appearance=old.appearance
    assert front.features.removeFeatures.add(old)
    assert front.bRepBodies.count==0
    b=m.slab(front,'Moldura alinhada ao corpo / sem orelhas laterais',174.22,108.94,5.97,-1.8,.5,0,-3)
    b.appearance=appearance
    m.fillet_edges(front,b,.5,'Arredondamento frontal continuo / 0.5 mm',0)
    m.cut_round(front,'Vidro preservado / janela 164.88 x 99.77',164.88,99.77,.6,0,0,.1,-3.3)
    m.circles(front,'MIC preservados / furos 1.6 mm',[(x,-51,1.6) for x in [-68,76]],.1,-3.3,OP.CutFeatureOperation)
    m.cut_round(front,'Flex touch preservado / pele frontal 0.7 mm',19.2,2,.4,63.99,-50.25,-.7,-2.5)
    assert front.bRepBodies.count==1
    assert front.bRepBodies.item(0).isSolid and front.bRepBodies.item(0).lumps.count==1
    for s in front.sketches:s.isLightBulbOn=False
    front.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','flush_bezel_v06',json.dumps({'width_mm':174.22,'height_mm':108.94,'corner_radius_mm':5.97,'centre_mm':[-1.8,.5],'depth_mm':3,'shell_section_z_mm':-3.03,'section_gap_mm':.03}))
    print('CORRECTED bezel outer profile matches shell front: 174.22 x 108.94, R5.97, centre -1.8 / 0.5 mm')
    print('Glass opening, both MIC ports and hidden touch flex relief retained')

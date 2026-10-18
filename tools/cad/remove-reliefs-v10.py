import adsk.core,adsk.fusion,json
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);assert 'Mesa V10' in app.activeDocument.name
 if d.attributes.itemByName('NP2','flat_floor_v10'):return
 c=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 for label in ['esquerdo','direito']:
  f=c.features.combineFeatures.itemByName('Alojamento raso plug interno '+label+' / exterior preservado');assert f and f.deleteMe()
  bf=c.features.baseFeatures.itemByName('Ferramenta rebaixo interno '+label+' / piso 1.2 mm');assert bf and bf.deleteMe()
 assert c.bRepBodies.count==1 and c.bRepBodies.item(0).lumps.count==1
 d.attributes.add('NP2','flat_floor_v10',json.dumps({'reliefs_removed':True,'floor_nominal_mm':2.4,'additional_height_mm':4,'internal_mating_plugs':'pending real dimensions; previously oversized reference intersects smooth floor'}))
 print('Internal reliefs removed; smooth nominal 2.4 mm floor restored; outer dimensions preserved')

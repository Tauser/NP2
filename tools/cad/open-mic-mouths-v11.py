import adsk.core,adsk.fusion,importlib.util,json
spec=importlib.util.spec_from_file_location('m','D:/Projetos/NP2/tools/cad/build-case.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def run(_context: str):
 app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);assert 'Mesa V11' in app.activeDocument.name
 shell=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
 if not d.attributes.itemByName('NP2','mic_mouths_v11'):
  for x in [-68,68]:m.cut_round(shell,'MIC / boca aberta para cartucho removivel',7,4.8,.1,x,-53.5,-3.02,-.62)
 d.attributes.add('NP2','mic_mouths_v11','no retaining front lip; bezel and screw stop retain cartridge')
 data=json.loads(d.attributes.itemByName('NP2','removable_mics_v11').value)
 data['extraction']='screen/bezel/screw removed; pull forward +native Z; tested at 0.2,0.5,1,2,5,10,20 mm'
 d.attributes.add('NP2','removable_mics_v11',json.dumps(data))
 with open('D:/Projetos/NP2/mechanical/concept-v11/mic-design.json','w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
 print('Full open inlet mouths; no front lip catching cartridge on extraction')

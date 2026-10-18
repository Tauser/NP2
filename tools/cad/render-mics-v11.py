"""Clean native CAD previews: separate ducts and their open print faces."""
import adsk.core,adsk.fusion,importlib.util,math
spec=importlib.util.spec_from_file_location('helpers','D:/Projetos/NP2/tools/cad/build-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def rotation(angle):
 t=adsk.core.Matrix3D.create();t.setToRotation(angle,adsk.core.Vector3D.create(0,1,0),m.pt(0,0,0));return t

def scene(app,title,items,filename,eye,target):
 doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType);doc.name=title
 d=adsk.fusion.Design.cast(app.activeProduct);d.designType=adsk.fusion.DesignTypes.DirectDesignType
 d.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
 lib=next(l for l in app.materialLibraries if l.appearances.itemById('Prism-374'))
 for i,(name,t,rgb) in enumerate(items):
  o=d.rootComponent.occurrences.addNewComponent(adsk.core.Matrix3D.create());o.component.name=name
  b=o.component.bRepBodies.add(t);b.appearance=m.appearance(d,lib,'Prism-374',name+str(i),rgb)
  o.component.isOriginFolderLightBulbOn=False
 d.rootComponent.isOriginFolderLightBulbOn=False;d.activateRootComponent()
 app.userInterface.activeSelections.clear()
 cam=adsk.core.Camera.create();cam.eye=m.pt(*eye);cam.target=m.pt(*target)
 cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isFitView=True;cam.isSmoothTransition=False
 app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
 assert app.activeViewport.saveAsImageFile('D:/Projetos/NP2/mechanical/concept-v11/'+filename,1600,1200)
 return doc

def run(_context: str):
 app=adsk.core.Application.get();assembly=app.activeDocument;d=adsk.fusion.Design.cast(app.activeProduct)
 assert 'Mesa V11' in assembly.name
 root=d.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
 shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
 duct=next(o for o in root.occurrences if o.component.name.startswith('09 /'))
 items=[('Corpo / assentos abertos',mgr.copy(shell.component.bRepBodies.item(0).createForAssemblyContext(shell)),(105,109,115))]
 for b in duct.component.bRepBodies:
  t=mgr.copy(b.createForAssemblyContext(duct))
  shift=adsk.core.Matrix3D.create();shift.translation=adsk.core.Vector3D.create(0,1.3,1.2);mgr.transform(t,shift)
  items.append((b.name+' / deslocado apenas para vista explodida',t,(67,135,186)))
 scene(app,'NP2 V11 / vista explodida dos cartuchos / nao imprimir',items,'np2-mic-removable.png',(0,200,140),(0,40,35))
 assembly.activate()
 items=[]
 for i,b in enumerate(sorted(list(duct.component.bRepBodies),key=lambda b:b.boundingBox.minPoint.x)):
  t=mgr.copy(b);mgr.transform(t,rotation(-math.pi/2 if ' A /' in b.name else math.pi/2))
  bb=t.boundingBox;shift=adsk.core.Matrix3D.create()
  shift.translation=adsk.core.Vector3D.create(i*2.5-bb.minPoint.x,-bb.minPoint.y,-bb.minPoint.z);mgr.transform(t,shift)
  items.append((b.name,t,(65,132,188) if ' A /' in b.name else (216,150,65)))
 scene(app,'NP2 V11 / metades com canal para cima / revisao',items,'np2-duct-halves.png',(110,-100,135),(40,8,1))
 assembly.activate();print('Exploded removable cartridges and flat-backed open half previews saved')

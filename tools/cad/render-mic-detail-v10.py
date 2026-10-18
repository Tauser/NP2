"""Render both ducts and a true X=68 section of the exported CAD solids."""
import adsk.core,adsk.fusion,importlib.util
spec=importlib.util.spec_from_file_location('helpers','D:/Projetos/NP2/tools/cad/build-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def run(_context: str):
 app=adsk.core.Application.get();assembly=app.activeDocument
 assert 'Mesa V10' in assembly.name
 d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent;mgr=adsk.fusion.TemporaryBRepManager.get()
 out='D:/Projetos/NP2/mechanical/concept-v10'
 camera=app.activeViewport.camera;visibility=[(o,o.isLightBulbOn) for o in root.occurrences]
 assert d.activateRootComponent();app.userInterface.activeSelections.clear()
 select=app.userInterface.commandDefinitions.itemById('SelectCommand')
 if select:select.execute()
 for o,_ in visibility:o.isLightBulbOn=o.component.name.startswith(('02 /','REF / Juntas'))
 cam=adsk.core.Camera.create();cam.eye=adsk.core.Point3D.create(0,19,14);cam.target=adsk.core.Point3D.create(0,2.8,2.2)
 cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.isFitView=True;cam.isSmoothTransition=False
 app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
 assert app.activeViewport.saveAsImageFile(out+'/np2-mic-ducts.png',1600,1200)
 for o,on in visibility:o.isLightBulbOn=on
 app.activeViewport.camera=camera
 shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'));inv=shell.transform2.copy();inv.invert()
 clip=mgr.createBox(adsk.core.OrientedBoundingBox3D.create(m.pt(70,-50.5,-8.4),adsk.core.Vector3D.create(1,0,0),adsk.core.Vector3D.create(0,1,0),.4,1.7,1.72))
 items=[]
 for o in root.allOccurrences:
  name=o.fullPathName
  label=None
  if o.component.name.startswith('02 /'):label='Corpo e duto / corte real X=68';color=(95,146,201)
  elif o.component.name.startswith('01 /'):label='Moldura e entrada frontal';color=(150,157,160)
  elif o.component.name.startswith('REF / Juntas'):label='Espuma / referencia comprimida';color=(224,143,47)
  elif name.startswith('REF OFICIAL') and '+MIC1_ASM:' in name:label='MIC1 oficial / envelope';color=(190,192,195)
  elif name.startswith('REF OFICIAL') and '+BOARD:' in name:label='PCB oficial / corte';color=(66,139,81)
  if not label:continue
  for b in o.component.bRepBodies:
   t=mgr.copy(b.createForAssemblyContext(o));mgr.transform(t,inv)
   assert mgr.booleanOperation(t,clip,adsk.fusion.BooleanTypes.IntersectionBooleanType)
   if t.volume>.000001:items.append((label,t,color))
 doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType);doc.name='NP2 - V10 / corte de revisao do duto MIC / nao imprimir'
 detail=adsk.fusion.Design.cast(app.activeProduct);detail.designType=adsk.fusion.DesignTypes.DirectDesignType
 detail.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
 lib=next(l for l in app.materialLibraries if l.appearances.itemById('Prism-374'))
 for i,(label,t,color) in enumerate(items):
  o=detail.rootComponent.occurrences.addNewComponent(adsk.core.Matrix3D.create());o.component.name=label
  b=o.component.bRepBodies.add(t);b.appearance=m.appearance(detail,lib,'Prism-374','Corte '+str(i),color)
  o.component.isOriginFolderLightBulbOn=False
 detail.rootComponent.isOriginFolderLightBulbOn=False
 assert detail.activateRootComponent()
 cam=adsk.core.Camera.create();cam.eye=m.pt(45,-50.5,-8.4);cam.target=m.pt(68,-50.5,-8.4)
 cam.upVector=adsk.core.Vector3D.create(0,0,1);cam.cameraType=adsk.core.CameraTypes.OrthographicCameraType
 cam.isFitView=True;cam.isSmoothTransition=False
 app.activeViewport.camera=cam;adsk.doEvents();app.activeViewport.refresh();adsk.doEvents()
 assert app.activeViewport.saveAsImageFile(out+'/np2-mic-section.png',1600,1200)
 assert detail.exportManager.execute(detail.exportManager.createFusionArchiveExportOptions(out+'/NP2-v10-corte-duto-revisao.f3d'))
 assembly.activate()
 print('True duct section and two-duct overview saved; assembly restored')

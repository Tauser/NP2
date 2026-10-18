"""Reuse the physical fit verifier for the V06 document and report directory."""
def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/verify-temp-v05.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace("'V05'","'V06'").replace('concept-v05','concept-v06')
    namespace={};exec(compile(source,path,'exec'),namespace)
    namespace['run'](_context)
    import adsk.core, adsk.fusion, json
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct);root=d.rootComponent
    mgr=adsk.fusion.TemporaryBRepManager.get()
    panel=next(o for o in root.occurrences if o.component.name.startswith('01 /')).transform2
    inverse=panel.copy();inverse.invert()
    shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
    case=mgr.copy(shell.component.bRepBodies.item(0).createForAssemblyContext(shell));mgr.transform(case,inverse)
    keys=next(o for o in root.occurrences if o.component.name.startswith('06 /'))
    pressed=[]
    for body in keys.component.bRepBodies:
        temp=mgr.copy(body.createForAssemblyContext(keys));mgr.transform(temp,inverse)
        t=adsk.core.Matrix3D.create();t.translation=adsk.core.Vector3D.create(0,-.05,0);mgr.transform(temp,t)
        intersection=mgr.copy(temp)
        assert mgr.booleanOperation(intersection,case,adsk.fusion.BooleanTypes.IntersectionBooleanType)
        volume=intersection.volume*1000
        assert volume<.01,'Volume key pressed into case'
        pressed.append({'body':body.name,'reference_travel_mm':.5,'intersection_mm3':volume})
    report='D:/Projetos/NP2/mechanical/concept-v06/fit-check.json'
    with open(report,encoding='utf8') as f:data=json.load(f)
    data['volume_pressed_pose_checks']=pressed
    with open(report,'w',encoding='utf8') as f:json.dump(data,f,ensure_ascii=False,indent=2)
    print('VOLUME PRESSED POSES',pressed)

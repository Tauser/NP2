"""Thin-bezel self-supporting NP2 desk enclosure; rear audio, no separate base."""
import adsk.core, adsk.fusion, math, sys, importlib.util
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('case','D:/Projetos/NP2/tools/cad/build-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
spec=importlib.util.spec_from_file_location('controls','D:/Projetos/NP2/tools/cad/add-controls.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
P=m.P;V=m.V;OP=m.OP
W,H,TILT,REAR_BOTTOM=176,110,18,78
SPEAKER_W,SPEAKER_H,SPEAKER_D=100,45,20  # User measurements in mm, 2026-10-09.
Y_BOTTOM=-H/2+REAR_BOTTOM*math.tan(math.radians(TILT))
Y_TOP=H/2
SLOPE=(REAR_BOTTOM-24)/(Y_TOP-Y_BOTTOM)
INTERCEPT=-24-SLOPE*Y_TOP
MID_Y=(Y_TOP+Y_BOTTOM)/2
MID_Z=(-24-REAR_BOTTOM)/2
REAR_H=math.hypot(Y_TOP-Y_BOTTOM,REAR_BOTTOM-24)

def rear_plane(comp):
    s=comp.sketches.add(comp.xYConstructionPlane);s.name='Referencia plano traseiro / 3 pontos'
    s.is3D=True
    points=[]
    for x,y,z in [(-10,0,INTERCEPT),(10,0,INTERCEPT),(0,20,INTERCEPT+20*SLOPE)]:
        points.append(s.sketchPoints.add(m.pt(x,y,z)))
    pi=comp.constructionPlanes.createInput();pi.setByThreePoints(*points)
    p=comp.constructionPlanes.add(pi);p.name='Plano da tampa inclinada';p.isLightBulbOn=False;s.isLightBulbOn=False
    return p

def rear_sketch(comp,name,offset=0):
    p=rear_plane(comp)
    if offset:
        pi=comp.constructionPlanes.createInput();pi.setByOffset(p,V.createByString(f'{offset} mm'))
        p=comp.constructionPlanes.add(pi);p.isLightBulbOn=False
    s=comp.sketches.add(p);s.name=name;s.isComputeDeferred=True
    return s

def rear_outline(s,w,h,r,dx=0,dy=0):
    center=s.modelToSketchSpace(m.pt(0,MID_Y,MID_Z))
    m.rounded(s,w,h,r,center.x*10+dx,center.y*10+dy)

def run(_context: str):
    app=adsk.core.Application.get()
    doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name='NP2 - Mesa V03 / sem base / audio traseiro'
    d=adsk.fusion.Design.cast(app.activeProduct);d.designType=adsk.fusion.DesignTypes.ParametricDesignType
    d.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    root=d.rootComponent;root.isOriginFolderLightBulbOn=False
    for name,expr,desc in [('wall','2.4 mm','Parede da carcaca'),('bezel','3 mm','Moldura frontal'),
                           ('rear_cover','2.4 mm','Tampa traseira'),('pcb_holes_x','156 mm','Entre furos da foto'),
                           ('pcb_holes_y','89 mm','Entre furos; PCB total 97 mm'),
                           ('speaker_width_ref','100 mm','Medido pelo usuario'),
                           ('speaker_height_ref','45 mm','Medido pelo usuario'),
                           ('speaker_depth','20 mm','Medido pelo usuario'),
                           ('speaker_clearance_ref','2 mm','Folga por lado do berco')]:
        d.userParameters.add(name,V.createByString(expr),'mm',desc)
    transform=adsk.core.Matrix3D.create()
    transform.setToRotation(math.radians(90-TILT),adsk.core.Vector3D.create(1,0,0),P.create(0,0,0))
    transform.translation=adsk.core.Vector3D.create(0,H/20*math.sin(math.radians(TILT)),H/20*math.cos(math.radians(TILT)))
    lib=next(l for l in app.materialLibraries if l.appearances.itemById('Prism-374'))
    paints={
      'shell':m.appearance(d,lib,'Prism-374','NP2 grafite',(44,48,54)),
      'rear':m.appearance(d,lib,'Prism-374','NP2 grafite traseiro',(54,58,65)),
      'black':m.appearance(d,lib,'Prism-374','NP2 vidro preto',(8,11,16)),
      'screen':m.appearance(d,lib,'Prism-374','NP2 tela azul',(18,35,48)),
      'keys':m.appearance(d,lib,'Prism-374','NP2 teclas',(139,151,158)),
      'pcb':m.appearance(d,lib,'Prism-374','NP2 PCB',(20,77,113)),
    }
    objects=[]
    def comp(name):
        occ,obj=m.component(root,name,transform);objects.append(occ);return obj
    front=comp('01 / Moldura fina / microfones frontais')
    b=m.slab(front,'Frente 176 x 110 R6',W,H,6,0,0,0,'-bezel');b.appearance=paints['shell']
    m.fillet_edges(front,b,.5,'Borda de toque suave',0)
    m.cut_round(front,'Janela vidro / folga 0.3 mm por lado',164.88,99.77,.6,0,0,.1,-3.3)
    m.circles(front,'MIC 1 e MIC 2 / borda inferior',[(x,-51,1.6) for x in [-68,73]],.1,-3.3,OP.CutFeatureOperation)
    display=comp('REF / Vidro 164.28 x 99.17 / area ativa assimetrica')
    b=m.slab(display,'Vidro do display',164.28,99.17,.25,0,0,-.65,-2.15);b.appearance=paints['black']
    b=m.slab(display,'Area ativa 154.58 x 86.42',154.58,86.42,.1,-1.795,2.355,-.6,-.04);b.appearance=paints['screen']
    shell=comp('02 / Corpo em cunha / apoio direto na mesa')
    s=c.oriented(shell,shell.yZConstructionPlane,-W/2,'Perfil lateral / piso horizontal em montagem')
    # Use modelToSketchSpace so plane axis conventions cannot reverse the shape.
    verts=[(-54,-3),(55,-3),(55,-24),(Y_BOTTOM,-REAR_BOTTOM)]
    for i,(y,z) in enumerate(verts):
        y2,z2=verts[(i+1)%len(verts)]
        s.sketchCurves.sketchLines.addByTwoPoints(s.modelToSketchSpace(m.pt(-W/2,y,z)),s.modelToSketchSpace(m.pt(-W/2,y2,z2)))
    b=m.extrude(shell,s,W,OP.NewBodyFeatureOperation,'Volume externo da cunha').bodies.item(0);b.appearance=paints['shell']
    # Remove front and back faces when hollowing, leaving walls and a level floor.
    faces=adsk.core.ObjectCollection.create()
    for f in b.faces:
        n=f.geometry.normal if f.geometry.objectType=='adsk::core::Plane' else None
        if n and (abs(n.z)>0.99 or (abs(n.x)<.01 and abs(n.z)>.7 and abs(n.y)>.4)):
            faces.add(f)
    assert faces.count==2,f'Expected front/rear, got {faces.count}'
    inp=shell.features.shellFeatures.createInput(faces,False);inp.insideThickness=V.createByString('wall')
    sf=shell.features.shellFeatures.add(inp);sf.name='Paredes 2.4 / frente e traseira abertas'
    # Chamfer-like narrow outer edge radii along the two top side edges.
    # Actual connector apertures are generous access windows pending connector envelopes.
    for side in [-1,1]:
        c.side(shell,side*(W/2+.1),'Acesso lateral / USB e expansao',0,-15,38,14,2,-side*2.8,OP.CutFeatureOperation)
    # Volume on the top; power on the right next to original switch.
    for x in [36,54]:
        c.top(shell,55.1,'Janela tecla volume',x,-14,9.6,7.6,3.6,2.7,OP.CutFeatureOperation)
    c.side(shell,W/2+.1,'Janela chave ON OFF original',37.82,-9,16,7.2,2.4,-2.7,OP.CutFeatureOperation)
    rear=comp('03 / Tampa traseira / grelha alto-falante + ventilacao')
    s=rear_sketch(rear,'Perfil tampa traseira')
    rear_outline(s,W,REAR_H,5)
    b=m.extrude(rear,s,'-rear_cover',OP.NewBodyFeatureOperation,'Tampa traseira removivel').bodies.item(0);b.appearance=paints['rear']
    s=rear_sketch(rear,'Grelha acustica traseira',.1)
    cc=s.modelToSketchSpace(m.pt(0,MID_Y,MID_Z))
    for row in range(11):
        for col in range(31):
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+(col-15)*3.2,cc.y*10+(row-5)*3.2),.1)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'341 perfuracoes de audio traseiras')
    s=rear_sketch(rear,'Ventilacao superior e inferior',.1)
    for yy in [-34,34]:
        for xx in range(-40,41,8):rear_outline(s,3,12,1.5,xx,yy)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'22 fendas de ventilacao')
    # Rear screws; bosses are modeled separately for later insert fit checks.
    s=rear_sketch(rear,'4 parafusos tampa',.1)
    for xx in [-78,78]:
        for yy in [-REAR_H/2+10,REAR_H/2-10]:
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+xx,cc.y*10+yy),.16)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'4 furos M3 tampa / diametro a validar')
    pcb=comp('REF / PCB 164 x 97 / pitch furos 156 x 89')
    b=m.slab(pcb,'PCB / espessura provisória',164,97,1,0,0,-6,-1.6);b.appearance=paints['pcb']
    m.circles(pcb,'Furos PCB / diametro a medir',[(x,y,3.2) for x in [-78,78] for y in [-44.5,44.5]],-5.9,-1.8,OP.CutFeatureOperation)
    mount=comp('04 / Apoios PCB / distancias da foto')
    for x in [-83.5,83.5]:
        b=m.slab(mount,'Trilho PCB',8,98,1.5,x,0,-9,-2);b.appearance=paints['shell']
    m.circles(mount,'4 apoios placa',[(x,y,7) for x in [-78,78] for y in [-44.5,44.5]],-7.6,-3.4,OP.JoinFeatureOperation)
    m.circles(mount,'4 pilotos placa / ajustar para M2.5',[(x,y,2.2) for x in [-78,78] for y in [-44.5,44.5]],-7.5,-3.6,OP.CutFeatureOperation)
    m.cut_round(mount,'Alivio para chave original',10,16,1,84,37.82,-7.8,-3.6)
    speaker=comp('REF / Alto-falante TRASEIRO / 100 x 45 x 20 medido pelo usuario')
    s=rear_sketch(speaker,'Envelope alto-falante / 100 x 45 x 20',5)
    rear_outline(s,SPEAKER_W,SPEAKER_H,3)
    b=m.extrude(speaker,s,'speaker_depth',OP.NewBodyFeatureOperation,'Alto-falante / envelope medido').bodies.item(0);b.appearance=paints['black']
    holder=comp('05 / Berco traseiro / alojamento 104 x 49 x 24')
    s=rear_sketch(holder,'Berco acustico',3)
    rear_outline(s,108,53,4)
    rear_outline(s,104,49,2)
    m.finish(s)
    ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,V.createByString('24 mm'),OP.NewBodyFeatureOperation)
    f.name='Berco traseiro aberto';f.bodies.item(0).appearance=paints['shell'];s.isLightBulbOn=False
    s=rear_sketch(holder,'Assento periférico / vedacao compressível',3)
    rear_outline(s,108,53,4);rear_outline(s,94,39,2)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,V.createByString('2 mm'),OP.JoinFeatureOperation)
    f.name='Assento do alto-falante';s.isLightBulbOn=False
    keys=comp('06 / Volume + e - / teclas no topo')
    for x,label in [(36,'Volume menos'),(54,'Volume mais')]:
        f=c.top(keys,56.4,label,x,-14,9,7,3.4,3.1,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['keys']
        c.top(keys,53.7,label+' flange',x,-14,12,10,2,1.2,OP.JoinFeatureOperation)
        c.top(keys,53,label+' haste',x,-14,3,3,.5,5.4,OP.JoinFeatureOperation)
        c.top(keys,56.5,label+' sinal horizontal',x,-14,3.5,.6,.25,.35,OP.CutFeatureOperation)
        if x==54:c.top(keys,56.5,label+' sinal vertical',x,-14,.6,3.5,.25,.35,OP.CutFeatureOperation)
    tact=comp('07 / Suporte dos switches volume / eletrica pendente')
    f=c.top(tact,46.3,'Suporte tact switches',45,-14,34,16,2,2.4,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['shell']
    for x in [36,54]:c.top(tact,46.4,'Nicho tact switch',x,-14,6.4,6.4,.4,1.1,OP.CutFeatureOperation)
    power=comp('08 / Cursor lateral ON OFF / aciona chave da placa')
    f=c.side(power,88.2,'Tecla externa ON OFF',37.82,-9,10,6.6,2.2,1.7,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['keys']
    c.side(power,83,'Ponte do cursor ate a chave',37.82,-9,5,4,.5,6,OP.JoinFeatureOperation)
    c.side(power,85,'Flange de retencao',37.82,-9,18,10,2,1.2,OP.JoinFeatureOperation)
    c.side(power,82,'Garfo chave / medir lingueta',37.82,-9,5,4,.5,2.2,OP.JoinFeatureOperation)
    c.side(power,81.9,'Rasgo para lingueta original',37.82,-9,2,2,.3,2.3,OP.CutFeatureOperation)
    for b in power.bRepBodies:b.appearance=paints['keys']
    # Duct seats in the lower rim, rear path to actual microphones remains measured.
    mics=comp('09 / Bicos MIC frontais / dutos a medir')
    for x in [-68,73]:
        s=m.sketch(mics,-3,'Bocal MIC')
        for diam in [3.6,2]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(x,-51),diam/20)
        m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
        f=mics.features.extrudeFeatures.addSimple(ring,V.createByString('-3 mm'),OP.NewBodyFeatureOperation)
        f.bodies.item(0).appearance=paints['shell'];s.isLightBulbOn=False
    for o in objects:
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False
        o.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for o in root.occurrences:
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    root.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','revision','V03: sem base separada, audio traseiro, moldura fina, mesa em Z=0')
    d.attributes.add('NP2','fit_status','CONCEITO: medir audio, alturas PCB, conectores, lingueta ONOFF e dutos MIC; fixacoes finais pendentes')
    print('CREATED V03',root.occurrences.count,'components',d.timeline.count,'features')
    print('WEDGE',W,H,'mm; tilt',TILT,'deg; rear bottom local Y',Y_BOTTOM,'floor footprint',REAR_BOTTOM/math.cos(math.radians(TILT)))
    print('CAMENUM',[a for a in dir(adsk.core.ViewOrientations) if 'Orientation' in a])

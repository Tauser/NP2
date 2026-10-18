"""V04: compact vertical rear, asymmetric bezel and rounded lofted body."""
import adsk.core, adsk.fusion, math, sys, importlib.util
sys.path.insert(0,'D:/Projetos/NP2/tools/cad')
spec=importlib.util.spec_from_file_location('case','D:/Projetos/NP2/tools/cad/build-case.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
spec=importlib.util.spec_from_file_location('controls','D:/Projetos/NP2/tools/cad/add-controls.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
P=m.P;V=m.V;OP=m.OP
W,H,TILT,REAR_BOTTOM=174.28,110,18,55
CX=-1.8
RIGHT=CX+W/2
REAR_W=W-4
AUDIO_Y=-12
SPEAKER_W,SPEAKER_H,SPEAKER_D=100,45,20  # User measurements in mm, 2026-10-09.
Y_BOTTOM=-H/2+REAR_BOTTOM*math.tan(math.radians(TILT))
Y_TOP=H/2
SLOPE=math.tan(math.radians(TILT))
INTERCEPT=-REAR_BOTTOM-SLOPE*Y_BOTTOM
REAR_TOP=-(INTERCEPT+SLOPE*Y_TOP)
MID_Y=(Y_TOP+Y_BOTTOM)/2
MID_Z=(-REAR_TOP-REAR_BOTTOM)/2
REAR_H=math.hypot(Y_TOP-Y_BOTTOM,REAR_BOTTOM-REAR_TOP)

def rear_plane(comp):
    s=comp.sketches.add(comp.xYConstructionPlane);s.name='Referencia plano traseiro / 3 pontos'
    s.is3D=True
    points=[]
    for x,y,z in [(-10,0,INTERCEPT),(10,0,INTERCEPT),(0,20,INTERCEPT+20*SLOPE)]:
        points.append(s.sketchPoints.add(m.pt(x,y,z)))
    pi=comp.constructionPlanes.createInput();pi.setByThreePoints(*points)
    p=comp.constructionPlanes.add(pi);p.name='Plano traseiro vertical em montagem';p.isLightBulbOn=False;s.isLightBulbOn=False
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
    doc.name='NP2 - Mesa V04 / traseira reta / corpo arredondado'
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
    b=m.slab(front,'Frente assimetrica / plastico esquerdo 6.8 e direito 3.2 mm',W,H,6,CX,0,0,'-bezel');b.appearance=paints['shell']
    m.fillet_edges(front,b,.5,'Borda de toque suave',0)
    m.cut_round(front,'Janela vidro / folga 0.3 mm por lado',164.88,99.77,.6,0,0,.1,-3.3)
    m.circles(front,'MIC 1 e MIC 2 / borda inferior',[(x,-51,1.6) for x in [-68,73]],.1,-3.3,OP.CutFeatureOperation)
    display=comp('REF / Vidro 164.28 x 99.17 / area ativa assimetrica')
    b=m.slab(display,'Vidro do display',164.28,99.17,.25,0,0,-.65,-2.15);b.appearance=paints['black']
    b=m.slab(display,'Area ativa 154.58 x 86.42',154.58,86.42,.1,-1.795,2.355,-.6,-.04);b.appearance=paints['screen']
    shell=comp('02 / Corpo arredondado / traseira vertical compacta')
    sf=m.sketch(shell,-3,'Contorno frontal do corpo / R6')
    m.rounded(sf,W,109,6,CX,.5);m.finish(sf)
    front_section=sf
    sr=rear_sketch(shell,'Contorno traseiro menor / R6')
    rear_outline(sr,REAR_W,REAR_H,6,CX,0);m.finish(sr)
    inp=shell.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    inp.loftSections.add(sf.profiles.item(0));inp.loftSections.add(sr.profiles.item(0))
    lf=shell.features.loftFeatures.add(inp);lf.name='Corpo com quinas R6 / transicao suave para tampa menor'
    b=lf.bodies.item(0);b.appearance=paints['shell'];sf.isLightBulbOn=False;sr.isLightBulbOn=False
    # Remove front and back faces when hollowing, leaving walls and a level floor.
    faces=adsk.core.ObjectCollection.create()
    for f in b.faces:
        n=f.geometry.normal if f.geometry.objectType=='adsk::core::Plane' else None
        if n and (abs(n.z)>0.99 or (abs(n.x)<.01 and abs(n.z)>.9 and abs(n.y)>.2)):
            faces.add(f)
    assert faces.count==2,f'Expected front/rear, got {faces.count}'
    inp=shell.features.shellFeatures.createInput(faces,False);inp.insideThickness=V.createByString('wall')
    sf=shell.features.shellFeatures.add(inp);sf.name='Paredes 2.4 / frente e traseira abertas'
    # Only the three right USB ports need individual side accesses. No left window.
    # Y centres from reference image; Z/connector envelopes still require calipers.
    for label,y,w,h,z in [('USB A',0,15,8,-11),('USB C',-19.65,11,6,-10),('USB UART',-34.93,11,6,-10)]:
        c.side(shell,RIGHT+.2,'Acesso individual / '+label,y,z,w,h,1.4,-4,OP.CutFeatureOperation)
    # Volume on the top; power on the right next to original switch.
    for x in [36,54]:
        c.top(shell,55.1,'Janela tecla volume',x,-14,9.6,7.6,3.6,2.7,OP.CutFeatureOperation)
    c.side(shell,RIGHT+.2,'Janela chave ON OFF original',37.82,-9,16,7.2,2.4,-4,OP.CutFeatureOperation)
    rear=comp('03 / Tampa traseira / grelha alto-falante + ventilacao')
    s=rear_sketch(rear,'Perfil tampa traseira')
    rear_outline(s,REAR_W,REAR_H,6,CX,0)
    b=m.extrude(rear,s,'-rear_cover',OP.NewBodyFeatureOperation,'Tampa traseira removivel').bodies.item(0);b.appearance=paints['rear']
    s=rear_sketch(rear,'Grelha acustica traseira',.1)
    cc=s.modelToSketchSpace(m.pt(0,MID_Y,MID_Z))
    for row in range(11):
        for col in range(31):
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+(col-15)*3.2,cc.y*10+AUDIO_Y+(row-5)*3.2),.1)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'341 perfuracoes de audio traseiras')
    s=rear_sketch(rear,'Ventilacao superior e inferior',.1)
    for yy in [30]:
        for xx in range(-40,41,8):rear_outline(s,3,12,1.5,xx,yy)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'11 fendas superiores de ventilacao')
    # Rear screws; bosses are modeled separately for later insert fit checks.
    s=rear_sketch(rear,'4 parafusos tampa',.1)
    for xx in [-78,78]:
        for yy in [-REAR_H/2+10,REAR_H/2-10]:
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(cc.x*10+xx,cc.y*10+yy),.16)
    m.extrude(rear,s,-2.7,OP.CutFeatureOperation,'4 furos M3 tampa / diametro a validar')
    pcb=comp('REF / PCB 164 x 97 / pitch furos 156 x 89')
    b=m.slab(pcb,'PCB / espessura provisória',164,97,1,0,0,-6,-1.6);b.appearance=paints['pcb']
    m.circles(pcb,'Furos PCB / diametro a medir',[(x,y,3.2) for x in [-78,78] for y in [-44.5,44.5]],-5.9,-1.8,OP.CutFeatureOperation)
    mount=shell
    # Four short ribs, rather than wide rails obstructing the right edge connectors.
    for x in [-78,78]:
        for y in [-44.5,44.5]:
            rx=-83.4 if x<0 else 80.5
            b=m.slab(mount,'Nervura suporte PCB',10 if x<0 else 6.6,9,1,rx,y,-9,-2);b.appearance=paints['shell']
    m.circles(mount,'4 apoios placa',[(x,y,7) for x in [-78,78] for y in [-44.5,44.5]],-7.6,-3.4,OP.JoinFeatureOperation)
    m.circles(mount,'4 pilotos placa / ajustar para M2.5',[(x,y,2.2) for x in [-78,78] for y in [-44.5,44.5]],-7.5,-3.6,OP.CutFeatureOperation)
    speaker=comp('REF / Alto-falante TRASEIRO / 100 x 45 x 20 medido pelo usuario')
    s=rear_sketch(speaker,'Envelope alto-falante / 100 x 45 x 20',5)
    rear_outline(s,SPEAKER_W,SPEAKER_H,3,0,AUDIO_Y)
    b=m.extrude(speaker,s,'speaker_depth',OP.NewBodyFeatureOperation,'Alto-falante / envelope medido').bodies.item(0);b.appearance=paints['black']
    holder=comp('05 / Berco traseiro / alojamento 104 x 49 x 24')
    s=rear_sketch(holder,'Berco acustico',3)
    rear_outline(s,108,53,4,0,AUDIO_Y)
    rear_outline(s,104,49,2,0,AUDIO_Y)
    m.finish(s)
    ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,V.createByString('24 mm'),OP.NewBodyFeatureOperation)
    f.name='Berco traseiro aberto';f.bodies.item(0).appearance=paints['shell'];s.isLightBulbOn=False
    s=rear_sketch(holder,'Assento periférico / vedacao compressível',3)
    rear_outline(s,108,53,4,0,AUDIO_Y);rear_outline(s,94,39,2,0,AUDIO_Y)
    m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
    f=holder.features.extrudeFeatures.addSimple(ring,V.createByString('2 mm'),OP.JoinFeatureOperation)
    f.name='Assento do alto-falante';s.isLightBulbOn=False
    keys=comp('06 / Volume + e - / teclas no topo')
    for x,label in [(36,'Volume menos'),(54,'Volume mais')]:
        f=c.top(keys,56.4,label,x,-14,9,7,3.4,3.1,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['keys']
        c.top(keys,53.5,label+' pescoco',x,-14,5,5,1,2.3,OP.JoinFeatureOperation)
        c.top(keys,52.4,label+' flange',x,-14,12,10,2,1.2,OP.JoinFeatureOperation)
        c.top(keys,51.3,label+' haste',x,-14,3,3,.5,3.7,OP.JoinFeatureOperation)
        c.top(keys,56.5,label+' sinal horizontal',x,-14,3.5,.6,.25,.35,OP.CutFeatureOperation)
        if x==54:c.top(keys,56.5,label+' sinal vertical',x,-14,.6,3.5,.25,.35,OP.CutFeatureOperation)
    tact=shell
    f=c.top(tact,46.3,'Suporte tact switches / integrado no corpo',45,-14,42,8,2,2.4,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['shell']
    for x in [25.5,64.5]:c.top(tact,55,'Pilar suporte volume / ligado ao teto',x,-14,3,8,1,11,OP.JoinFeatureOperation)
    for x in [36,54]:c.top(tact,46.4,'Nicho tact switch',x,-14,6.4,6.4,.4,1.1,OP.CutFeatureOperation)
    power=comp('08 / Cursor lateral ON OFF / aciona chave da placa')
    f=c.side(power,RIGHT-.1,'Tecla externa ON OFF',37.82,-9,10,6.6,2.2,1.7,OP.NewBodyFeatureOperation);f.bodies.item(0).appearance=paints['keys']
    c.side(power,83,'Ponte do cursor ate a chave',37.82,-9,5,4,.5,RIGHT-82.8,OP.JoinFeatureOperation)
    c.side(power,81.3,'Flange de retencao / abaixo da placa e do apoio',34.5,-13,10,9,2,.9,OP.JoinFeatureOperation)
    c.side(power,82,'Garfo chave / medir lingueta',37.82,-9,5,4,.5,2.2,OP.JoinFeatureOperation)
    c.side(power,81.9,'Rasgo para lingueta original',37.82,-9,2,2,.3,2.3,OP.CutFeatureOperation)
    for b in power.bRepBodies:b.appearance=paints['keys']
    # Duct seats in the lower rim, rear path to actual microphones remains measured.
    mics=shell
    for x in [-68,73]:
        s=m.sketch(mics,-3,'Bocal MIC')
        for diam in [3.6,2]:s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(x,-51),diam/20)
        m.finish(s);ring=next(p for p in s.profiles if p.profileLoops.count==2)
        f=mics.features.extrudeFeatures.addSimple(ring,V.createByString('-3 mm'),OP.JoinFeatureOperation)
        f.bodies.item(0).appearance=paints['shell'];s.isLightBulbOn=False
    # Join mounting ribs to the shell and trim everything to its original outer loft.
    # This prevents hidden protrusions through the rounded exterior.
    s=rear_sketch(shell,'4 torres da tampa / integradas nas paredes',0)
    for xx in [-78,78]:
        for yy in [-REAR_H/2+10,REAR_H/2-10]:
            rear_outline(s,12 if xx<0 else 10,9,2,-81 if xx<0 else 79,yy)
    m.extrude(shell,s,8,OP.NewBodyFeatureOperation,'Torres de fixacao tampa / M3')
    s=rear_sketch(shell,'4 pilotos das torres da tampa',-.1)
    center=s.modelToSketchSpace(m.pt(0,MID_Y,MID_Z))
    for xx in [-78,78]:
        for yy in [-REAR_H/2+10,REAR_H/2-10]:
            s.sketchCurves.sketchCircles.addByCenterRadius(m.pt(center.x*10+xx,center.y*10+yy),.13)
    m.extrude(shell,s,8.2,OP.CutFeatureOperation,'Pilotos M3 tampa / ajustar conforme parafuso')
    bodies=adsk.core.ObjectCollection.create()
    main=shell.bRepBodies.item(0)
    for body in shell.bRepBodies:
        if body!=main:bodies.add(body)
    if bodies.count:
        ci=shell.features.combineFeatures.createInput(main,bodies)
        ci.operation=OP.JoinFeatureOperation
        cf=shell.features.combineFeatures.add(ci);cf.name='Apoios e suporte volume integrados ao corpo'
        main=cf.bodies.item(0)
    fs=m.sketch(shell,-2.9,'Envelope externo / tolerancia booleana 0.1 mm')
    m.rounded(fs,W+.2,109.2,6.1,CX,.5);m.finish(fs)
    rs=rear_sketch(shell,'Envelope traseiro / tolerancia booleana 0.1 mm',-.1)
    rear_outline(rs,REAR_W+.2,REAR_H+.2,6.1,CX,0);m.finish(rs)
    inp=shell.features.loftFeatures.createInput(OP.NewBodyFeatureOperation)
    inp.loftSections.add(fs.profiles.item(0));inp.loftSections.add(rs.profiles.item(0))
    outer=shell.features.loftFeatures.add(inp);outer.name='Envelope de acabamento / folga numerica 0.1 mm'
    tools=adsk.core.ObjectCollection.create();tools.add(outer.bodies.item(0))
    ci=shell.features.combineFeatures.createInput(main,tools);ci.operation=OP.IntersectFeatureOperation
    cf=shell.features.combineFeatures.add(ci);cf.name='Limitar apoios ao exterior do corpo'
    # The UART access reaches the lower support; clear it after joining the ribs.
    for label,y,w,h,z in [('USB UART',-34.93,11,6,-10)]:
        c.side(shell,RIGHT+.2,'Acabamento final / '+label,y,z,w,h,1.4,-4,OP.CutFeatureOperation)
    c.side(shell,RIGHT+.2,'Acabamento final / janela ON OFF',37.82,-9,16,7.2,2.4,-4,OP.CutFeatureOperation)
    c.side(shell,80.8,'Bolso INTERNO cursor / retencao e folga',34.5,-13,14.4,10,2,1.8,OP.CutFeatureOperation)
    for o in objects:
        if hasattr(o,'isGroundToParent'):o.isGroundToParent=False
        o.isGrounded=False
        o.transform2=transform
    if d.snapshots.hasPendingSnapshot:d.snapshots.add()
    for o in root.occurrences:
        o.component.isOriginFolderLightBulbOn=False;o.component.isConstructionFolderLightBulbOn=False
        for s in o.component.sketches:s.isLightBulbOn=False
    root.isConstructionFolderLightBulbOn=False
    d.attributes.add('NP2','revision','V04: traseira vertical menor, corpo arredondado R6, 3 USB individuais, borda direita fina')
    d.attributes.add('NP2','fit_status','CONCEITO: medir audio, alturas PCB, conectores, lingueta ONOFF e dutos MIC; fixacoes finais pendentes')
    print('CREATED V04',root.occurrences.count,'components',d.timeline.count,'features')
    print('WEDGE',W,H,'mm; tilt',TILT,'deg; rear bottom local Y',Y_BOTTOM,'floor footprint',REAR_BOTTOM/math.cos(math.radians(TILT)))
    print('CAMENUM',[a for a in dir(adsk.core.ViewOrientations) if 'Orientation' in a])

"""NP2 desktop enclosure concept. Fusion internal lengths are centimetres.
XY is the display plane; -Z points towards the rear. Dimensions below are mm.
Envelope sketches are fixed; named extrusion parameters remain editable.
This is a fit-check concept, not a manufacturing release.
"""
import adsk.core, adsk.fusion, math, json

P = adsk.core.Point3D
V = adsk.core.ValueInput
OP = adsk.fusion.FeatureOperations

def pt(x, y, z=0):
    return P.create(x/10, y/10, z/10)

def plane(comp, z):
    if abs(z) < 1e-8:
        return comp.xYConstructionPlane
    inp = comp.constructionPlanes.createInput()
    inp.setByOffset(comp.xYConstructionPlane, V.createByString(f'{z} mm'))
    obj = comp.constructionPlanes.add(inp)
    obj.isLightBulbOn = False
    return obj

def sketch(comp, z, name):
    s = comp.sketches.add(plane(comp,z))
    s.name = name
    s.isComputeDeferred = True
    return s

def rounded(s, w, h, r, cx=0, cy=0):
    x0,x1,y0,y1 = cx-w/2,cx+w/2,cy-h/2,cy+h/2
    lines=s.sketchCurves.sketchLines
    arcs=s.sketchCurves.sketchArcs
    seq=[((x0+r,y0),(x1-r,y0)),((x1,y0+r),(x1,y1-r)),
         ((x1-r,y1),(x0+r,y1)),((x0,y1-r),(x0,y0+r))]
    for a,b in seq:
        lines.addByTwoPoints(pt(*a),pt(*b))
    for c,start in [((x1-r,y0+r),(x1-r,y0)),((x1-r,y1-r),(x1,y1-r)),
                    ((x0+r,y1-r),(x0+r,y1)),((x0+r,y0+r),(x0,y0+r))]:
        arcs.addByCenterStartSweep(pt(*c),pt(*start),math.pi/2)

def finish(s):
    s.isComputeDeferred = False
    for c in s.sketchCurves:
        c.isFixed = True

def extrude(comp,s,depth,op,name):
    finish(s)
    profiles=adsk.core.ObjectCollection.create()
    for p in s.profiles:
        profiles.add(p)
    assert profiles.count, f'No closed profiles: {s.name}'
    expr=depth if isinstance(depth,str) else f'{depth} mm'
    if op in [OP.CutFeatureOperation,OP.IntersectFeatureOperation]:
        inp=comp.features.extrudeFeatures.createInput(profiles,op)
        inp.setDistanceExtent(False,V.createByString(expr))
        # Fusion otherwise cuts every intersecting body, including other components.
        inp.participantBodies=[b for b in comp.bRepBodies]
        f=comp.features.extrudeFeatures.add(inp)
    else:
        f=comp.features.extrudeFeatures.addSimple(profiles,V.createByString(expr),op)
    f.name=name
    s.isLightBulbOn=False
    return f

def slab(comp,name,w,h,r,cx,cy,z,depth):
    s=sketch(comp,z,name+' / perfil')
    rounded(s,w,h,r,cx,cy)
    return extrude(comp,s,depth,OP.NewBodyFeatureOperation,name).bodies.item(0)

def cut_round(comp,name,w,h,r,cx,cy,z,depth):
    s=sketch(comp,z,name+' / perfil')
    rounded(s,w,h,r,cx,cy)
    return extrude(comp,s,depth,OP.CutFeatureOperation,name)

def circles(comp,name,locations,z,depth,op):
    s=sketch(comp,z,name+' / perfil')
    for x,y,d in locations:
        s.sketchCurves.sketchCircles.addByCenterRadius(pt(x,y),d/20)
    return extrude(comp,s,depth,op,name)

def component(root,name,transform):
    occ=root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    if hasattr(occ,'isGroundToParent'):occ.isGroundToParent=False
    occ.isGrounded=False
    occ.component.name=name
    return occ,occ.component

def appearance(design,lib,source,name,rgb):
    a=design.appearances.addByCopy(lib.appearances.itemById(source),name)
    color=adsk.core.Color.create(*rgb,255)
    prop=a.appearanceProperties.itemById('opaque_albedo')
    if not prop:
        prop=a.appearanceProperties.itemById('generic_diffuse')
    if prop:
        adsk.core.ColorProperty.cast(prop).value=color
    else:
        a.color=color
    return a

def fillet_edges(comp,body,r,name,z=None):
    edges=adsk.core.ObjectCollection.create()
    for e in body.edges:
        bb=e.boundingBox
        if z is not None and abs(bb.minPoint.z-z/10)<1e-5 and abs(bb.maxPoint.z-z/10)<1e-5:
            edges.add(e)
    if edges.count:
        inp=comp.features.filletFeatures.createInput()
        inp.edgeSetInputs.addConstantRadiusEdgeSet(edges,V.createByString(f'{r} mm'),True)
        f=comp.features.filletFeatures.add(inp)
        f.name=name

def run(_context: str):
    app=adsk.core.Application.get()
    design=adsk.fusion.Design.cast(app.activeProduct)
    assert design and app.activeDocument.name.startswith('NP2 - Mesa'), 'Activate the NP2 concept document'
    root=design.rootComponent
    assert root.occurrences.count==0, 'Build only into the empty NP2 document'
    design.designType=adsk.fusion.DesignTypes.ParametricDesignType
    design.unitsManager.distanceDisplayUnits=adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    for name,expr,comment in [
        ('bezel_thickness','3 mm','Moldura frontal'),
        ('shell_depth','31 mm','Profundidade do corpo traseiro'),
        ('rear_thickness','2.4 mm','Tampa removivel'),
        ('base_thickness','8 mm','Base de mesa'),
        ('wall_nominal','2.4 mm','Referencia da parede; perfil fixo no script'),
        ('mic_port_diameter','1.8 mm','Referencia; posicao depende de medicao'),
        ('pcb_width_ref','164 mm','Medida da imagem fornecida'),
        ('pcb_height_ref','97 mm','Altura total da segunda imagem; 89 mm e o pitch dos furos'),
        ('screen_width_ref','164.28 mm','Vidro da imagem fornecida'),
        ('screen_height_ref','99.17 mm','Vidro da imagem fornecida'),
    ]:
        design.userParameters.add(name,V.createByString(expr),'mm',comment)
    angle=math.radians(72)
    trans=adsk.core.Matrix3D.create()
    trans.setToRotation(angle,adsk.core.Vector3D.create(1,0,0),P.create(0,0,0))
    trans.translation=adsk.core.Vector3D.create(0,3.1,(12+72*math.sin(angle)+36.4*math.cos(angle))/10)
    lib=next(l for l in app.materialLibraries if l.appearances.itemById('Prism-374'))
    graphite=appearance(design,lib,'Prism-374','NP2 / grafite fosco',(46,51,58))
    rearpaint=appearance(design,lib,'Prism-374','NP2 / grafite traseiro',(62,67,74))
    black=appearance(design,lib,'Prism-374','NP2 / preto profundo',(9,12,17))
    screenpaint=appearance(design,lib,'Prism-374','NP2 / display azul escuro',(17,36,52))
    rubber=appearance(design,lib,'Prism-130','NP2 / silicone preto',(20,22,24))
    pcbpaint=appearance(design,lib,'Prism-374','NP2 / PCB referencia',(21,73,108))
    accent=appearance(design,lib,'Prism-374','NP2 / cinza claro botoes',(158,167,171))
    all_panel=[]

    occ,front=component(root,'01 / Moldura frontal + grelha + portas MIC',trans);all_panel.append(occ)
    b=slab(front,'Moldura 184 x 144 R10',184,144,10,0,0,0,'-bezel_thickness');b.appearance=graphite
    fillet_edges(front,b,.65,'Borda frontal suave',0)
    cut_round(front,'Janela do vidro / folga 0.30 por lado',164.88,99.77,.6,0,10,.1,-3.3)
    holes=[]
    for row in range(5):
        for col in range(22):
            holes.append(((col-10.5)*3.3,-55+(row-2)*3.3,2.0))
    circles(front,'Grelha acustica / 110 perfuracoes',holes,.1,-3.3,OP.CutFeatureOperation)
    circles(front,'MIC esquerdo e direito / alinhar dutos',[(x,-48,1.8) for x in [-74,74]],.1,-3.3,OP.CutFeatureOperation)
    # Separate soft interface rings rather than claiming microphone alignment.
    for x in [-74,74]:
        s=sketch(front,-3,'MIC / apoio de tubo silicone')
        for d in [5.5,2.6]:s.sketchCurves.sketchCircles.addByCenterRadius(pt(x,-48),d/20)
        finish(s)
        ring=next(p for p in s.profiles if p.profileLoops.count==2)
        f=front.features.extrudeFeatures.addSimple(ring,V.createByString('-3 mm'),OP.NewBodyFeatureOperation)
        f.name='MIC / bocal frontal provisório';f.bodies.item(0).appearance=graphite;s.isLightBulbOn=False

    occ,shell=component(root,'02 / Corpo traseiro aberto / parede 2.4',trans);all_panel.append(occ)
    b=slab(shell,'Corpo traseiro',184,144,10,0,0,-3,'-shell_depth');b.appearance=rearpaint
    cut_round(shell,'Cavidade principal',179.2,139.2,7.6,0,0,-2.9,-31.3)
    # Four side access windows, generous until connector coordinates are measured.
    for x,cy in [(-89,10),(89,10)]:
        cut_round(shell,'Acesso lateral / conectores - confirmar',12,38,2,x,cy,-9,-14)
    # Structural pillars for rear-cover screws, clear of the glass.
    circles(shell,'4 pilares da tampa',[(x,y,7) for x in [-86,86] for y in [-65,65]],-3,-31,OP.JoinFeatureOperation)
    circles(shell,'4 pilotos M3 / ajustar inserto',[(x,y,2.6) for x in [-86,86] for y in [-65,65]],-33.9,8,OP.CutFeatureOperation)

    occ,rear=component(root,'03 / Tampa traseira removivel / ventilacao',trans);all_panel.append(occ)
    b=slab(rear,'Tampa traseira',184,144,10,0,0,-34,'-rear_thickness');b.appearance=rearpaint
    circles(rear,'4 passagens de parafuso M3',[(x,y,3.2) for x in [-86,86] for y in [-65,65]],-33.9,-2.6,OP.CutFeatureOperation)
    s=sketch(rear,-33.9,'Ventilacao superior / 12 fendas')
    for x in range(-44,45,8):rounded(s,3,20,1.5,x,45)
    extrude(rear,s,-2.6,OP.CutFeatureOperation,'Fendas traseiras superiores')
    s=sketch(rear,-33.9,'Entrada de ar inferior / 8 fendas')
    for x in range(-28,29,8):rounded(s,3,12,1.5,x,-26)
    extrude(rear,s,-2.6,OP.CutFeatureOperation,'Fendas traseiras inferiores')

    occ,display=component(root,'REF / Display Waveshare 7B / vidro 164.28 x 99.17',trans);all_panel.append(occ)
    b=slab(display,'Vidro / referencia dimensional',164.28,99.17,.3,0,10,-.65,-2.15);b.appearance=black
    b=slab(display,'Area ativa 154.58 x 86.42',154.58,86.42,.15,-1.795,12.355,-.60,-.04);b.appearance=screenpaint

    occ,pcb=component(root,'REF / PCB 164 x 97 / furos 156 x 89',trans);all_panel.append(occ)
    b=slab(pcb,'Envelope PCB / conferir altura de componentes',164,97,1,0,10,-6,-1.6);b.appearance=pcbpaint
    circles(pcb,'Furos referencia / diametro a medir',[(x,y,3.2) for x in [-78,78] for y in [-34.5,54.5]],-5.9,-1.8,OP.CutFeatureOperation)
    occ,mount=component(root,'04 / Suporte da placa / pitch 156 x 89',trans);all_panel.append(occ)
    # Plate-holder rails leave the bottom audio zone unobstructed.
    for x in [-84,84]:
        b=slab(mount,'Trilho lateral',8,113,2,x,10,-9,-2);b.appearance=graphite
    circles(mount,'4 apoios PCB provisórios',[(x,y,6.4) for x in [-78,78] for y in [-34.5,54.5]],-7.6,-3.4,OP.NewBodyFeatureOperation)
    circles(mount,'4 pilotos PCB',[(x,y,2.6) for x in [-78,78] for y in [-34.5,54.5]],-7.5,-3.6,OP.CutFeatureOperation)
    for b in mount.bRepBodies:b.appearance=graphite

    occ,audio=component(root,'05 / Alojamento do alto-falante / MEDIR PECA',trans);all_panel.append(occ)
    b=slab(audio,'Berco acustico 82 x 29 / provisório',82,29,3,0,-54,-4,-23);b.appearance=graphite
    cut_round(audio,'Cavidade alto-falante 76 x 27',76,27,2,0,-54,-3.9,-21.3)
    cut_round(audio,'Passagem de cabo audio',7,5,1,39,-51,-23,-4.2)
    occ,spk=component(root,'REF / Alto-falante 72 x 25 x 16 / dimensoes HIPOTESE',trans);all_panel.append(occ)
    b=slab(spk,'Envelope do alto-falante',72,25,3,0,-54,-6,-16);b.appearance=black
    for x in [-19,19]:
        f=circles(spk,'Membrana / visual de referencia',[(x,-54,19)],-5.5,-.5,OP.NewBodyFeatureOperation)
        f.bodies.item(0).appearance=rubber

    occ,base=component(root,'06 / Base de mesa + dois apoios',adsk.core.Matrix3D.create())
    b=slab(base,'Base 176 x 108 R12',176,108,12,0,54,0,'base_thickness');b.appearance=graphite
    fillet_edges(base,b,1.2,'Borda da base suave',8)
    for x in [-55,55]:
        b=slab(base,'Apoio traseiro / inclinado 18 graus',14,24,3,x,54,8,25);b.appearance=graphite
    occ,feet=component(root,'07 / Quatro pes de silicone',adsk.core.Matrix3D.create())
    for x in [-63,63]:
        for y in [18,90]:
            b=slab(feet,'Pe antiderrapante',22,14,6,x,y,-2,2);b.appearance=rubber

    for occ in all_panel:occ.transform2=trans
    if design.snapshots.hasPendingSnapshot:design.snapshots.add()
    for occ in root.allOccurrences:
        occ.component.isOriginFolderLightBulbOn=False
        occ.component.isConstructionFolderLightBulbOn=False
        for s in occ.component.sketches:s.isLightBulbOn=False
    design.attributes.add('NP2','status','CONCEITO SEM BATERIA - nao liberar para impressao antes de medir audio, PCB, botoes e conectores')
    design.attributes.add('NP2','speaker_placeholder_mm','72 x 25 x 16 - HIPOTESE')
    design.attributes.add('NP2','angle_deg','18 from vertical')
    cam=app.activeViewport.camera
    cam.eye=pt(240,-290,220);cam.target=pt(0,48,75);cam.upVector=adsk.core.Vector3D.create(0,0,1)
    cam.isFitView=False;cam.viewExtents=18;cam.isSmoothTransition=False
    app.activeViewport.camera=cam
    app.activeViewport.refresh()
    print('BUILT',root.occurrences.count,'components',design.timeline.count,'timeline entries')
    print('CONCEPT ONLY: speaker, PCB thickness/pitch, connectors, microphone ducts require physical measurements')

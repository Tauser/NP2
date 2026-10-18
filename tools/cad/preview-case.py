import adsk.core, adsk.fusion, json, os

def run(_context: str):
    app=adsk.core.Application.get()
    design=adsk.fusion.Design.cast(app.activeProduct)
    root=design.rootComponent
    issues=[]
    for i in range(design.timeline.count):
        item=design.timeline.item(i)
        if item.healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            issues.append((item.name,item.errorOrWarningMessage))
    print('HEALTH',json.dumps(issues))
    print('COMPONENTS',[(o.component.name,o.component.bRepBodies.count) for o in root.occurrences])
    bb=root.boundingBox
    print('BOUND_MM',[(bb.maxPoint.x-bb.minPoint.x)*10,(bb.maxPoint.y-bb.minPoint.y)*10,(bb.maxPoint.z-bb.minPoint.z)*10])
    app.activeViewport.visualStyle=adsk.core.VisualStyles.ShadedVisualStyle
    out='D:/Projetos/NP2/mechanical/concept-v01'
    os.makedirs(out,exist_ok=True)
    assert app.activeViewport.saveAsImageFile(out+'/np2-front.png',1600,1200)
    print('IMAGE',out+'/np2-front.png')

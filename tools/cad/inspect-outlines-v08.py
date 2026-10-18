import adsk.core, adsk.fusion
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    for o in d.rootComponent.occurrences:
        if not o.component.name.startswith(('01 /','02 /','03 /')):continue
        print('COMP',o.component.name)
        for s in o.component.sketches:
            if any(k in s.name for k in ['Contorno','Exterior continuo','Traseira exterior','Acabamento exterior','Traseira de acabamento','Moldura alinhada','Tampa plana']):
                print('SKETCH',s.name,'CURVES',s.sketchCurves.count,'MOVE DOC',s.move.__doc__)
        for f in o.component.features:
            if 'Bocal' in f.name:print('MIC FEATURE',f.name,f.timelineObject.index)

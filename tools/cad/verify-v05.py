import adsk.core, adsk.fusion, json, os
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct);root=d.rootComponent
    assert 'V05' in app.activeDocument.name
    items=adsk.core.ObjectCollection.create()
    for o in root.occurrences:items.add(o)
    inp=d.createInterferenceInput(items);inp.areCoincidentFacesIncluded=False
    result=d.analyzeInterference(inp);print('ALL INTERFERENCES',result.count)
    clashes=[];ignored=0
    for r in result:
        a=r.entityOne;b=r.entityTwo
        pa=a.assemblyContext.fullPathName if a.assemblyContext else a.parentComponent.name
        pb=b.assemblyContext.fullPathName if b.assemblyContext else b.parentComponent.name
        if pa.startswith('REF OFICIAL') and pb.startswith('REF OFICIAL'):
            ignored+=1;continue
        bb=r.interferenceBody.boundingBox
        item={'one':pa,'two':pb,'volume_mm3':r.interferenceBody.volume*1000,'bbox_mm':[[x*10 for x in bb.minPoint.asArray()],[x*10 for x in bb.maxPoint.asArray()]]}
        clashes.append(item)
    print('HARDWARE INTERNAL OVERLAPS IGNORED',ignored)
    print('CASE CLASHES',len(clashes))
    for c in clashes[:30]:print(json.dumps(c,ensure_ascii=False))
    health=[(d.timeline.item(i).name,d.timeline.item(i).errorOrWarningMessage) for i in range(d.timeline.count) if d.timeline.item(i).healthState!=adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState]
    print('HEALTH',health)
    out='D:/Projetos/NP2/mechanical/concept-v05';os.makedirs(out,exist_ok=True)
    with open(out+'/fit-check.json','w',encoding='utf8') as f:json.dump({'clashes':clashes,'timeline_issues':health,'hardware_internal_overlaps':ignored},f,ensure_ascii=False,indent=2)
    print('TRANSFORMS',[(o.component.name,o.transform2.asArray()) for o in root.occurrences])
    h=next(o for o in root.occurrences if o.component.name.startswith('REF OFICIAL'))
    print('HARDWARE BODY COUNT',sum(o.component.bRepBodies.count for o in h.component.allOccurrences))

import adsk.core, adsk.fusion
def run(_context: str):
    app=adsk.core.Application.get();d=adsk.fusion.Design.cast(app.activeProduct)
    assert 'V05' in app.activeDocument.name
    mgr=adsk.fusion.TemporaryBRepManager.get()
    hw=next(o for o in d.rootComponent.allOccurrences if o.component.bRepBodies.count and o.fullPathName.endswith('+BOARD:1'))
    case=next(o for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for o in [hw,case]:
        b=o.component.bRepBodies.item(0).createForAssemblyContext(o);t=mgr.copy(b)
        print('COPY',o.fullPathName,'solid',t.isSolid,'volume',t.volume,'BB',t.boundingBox.minPoint.asArray(),t.boundingBox.maxPoint.asArray())
        print('PROXY',b.boundingBox.minPoint.asArray(),b.boundingBox.maxPoint.asArray())
    a=mgr.copy(case.component.bRepBodies.item(0).createForAssemblyContext(case))
    b=mgr.copy(hw.component.bRepBodies.item(0).createForAssemblyContext(hw))
    ok=mgr.booleanOperation(a,b,adsk.fusion.BooleanTypes.IntersectionBooleanType)
    print('INTERSECTION',ok,a.volume if ok else None)

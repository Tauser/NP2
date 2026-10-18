import adsk.core,adsk.fusion
def run(_context: str):
 d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct);root=d.rootComponent
 shell=next(o for o in root.occurrences if o.component.name.startswith('02 /'))
 g=next(o for o in root.occurrences if o.component.name.startswith('REF / Juntas'))
 g.transform2=shell.transform2.copy()
 if d.snapshots.hasPendingSnapshot:d.snapshots.add()
 print('Compressed gasket references placed in assembly coordinates')

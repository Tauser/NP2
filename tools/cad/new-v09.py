import adsk.core,adsk.fusion,os
def run(_context: str):
 app=adsk.core.Application.get()
 out='D:/Projetos/NP2/mechanical/concept-v09';os.makedirs(out,exist_ok=True)
 path='D:/Projetos/NP2/mechanical/concept-v08/NP2-case-v08.f3d'
 doc=app.importManager.importToNewDocument(app.importManager.createFusionArchiveImportOptions(path));assert doc
 doc.name='NP2 - Mesa V09 / folga inferior compacta'
 print('V09 copied from preserved V08')

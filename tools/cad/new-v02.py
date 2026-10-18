import adsk.core
def run(_context: str):
    app=adsk.core.Application.get()
    doc=app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    doc.name='NP2 - Mesa / conceito V02 sem bateria + controles'
    print('CREATED',doc.name)

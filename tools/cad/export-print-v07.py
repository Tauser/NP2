"""V07 print layout keeps the taller integral-foot shell on the 350 mm bed."""
def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/export-print-v06.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('V06','V07').replace('v06','v07')
    source=source.replace("('03 /','03-tampa-berco.stl',(8,130))","('03 /','03-tampa-berco.stl',(8,150))")
    namespace={};exec(compile(source,path,'exec'),namespace)
    namespace['run'](_context)

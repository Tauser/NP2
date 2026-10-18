def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/export-print-v06.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V06','V10').replace('v06','v10')
 source=source.replace("('03 /','03-tampa-berco.stl',(8,130))","('03 /','03-tampa-berco.stl',(8,150))")
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

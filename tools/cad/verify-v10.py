def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/verify-v09.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V09','V10').replace('concept-v09','concept-v10')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

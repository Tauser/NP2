def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/check-access-v10.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('Mesa V10','Mesa V11').replace('concept-v10','concept-v11')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

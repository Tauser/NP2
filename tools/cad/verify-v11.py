def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/verify-temp-v05.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace("'V05'","'V11'").replace('concept-v05','concept-v11')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

from pathlib import Path
path=Path('D:/Projetos/NP2/tools/cad/package-print-v08.py')
source=path.read_text(encoding='utf8').replace('v08','v09').replace('V08','V09')
exec(compile(source,str(path),'exec'))

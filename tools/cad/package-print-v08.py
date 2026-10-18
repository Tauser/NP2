from pathlib import Path
path=Path('D:/Projetos/NP2/tools/cad/package-print-v06.py')
source=path.read_text(encoding='utf8').replace('v06','v08').replace('V06','V08')
source=source.replace("'bezel-seam-after.json'","'connector-access-after.json','np2-underside.png'")
exec(compile(source,str(path),'exec'))

"""Mesh and 350 mm plate verification for V07, plus insertion-path evidence."""
from pathlib import Path
path=Path('D:/Projetos/NP2/tools/cad/package-print-v06.py')
source=path.read_text(encoding='utf8').replace('v06','v07').replace('V06','V07')
source=source.replace("'bezel-seam-after.json'","'connector-access-after.json','np2-underside.png'")
exec(compile(source,str(path),'exec'))

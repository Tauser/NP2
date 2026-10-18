from pathlib import Path
path=Path('D:/Projetos/NP2/tools/cad/package-print-v06.py')
source=path.read_text(encoding='utf8').replace('v06','v11').replace('V06','V11')
source=source.replace('len(meshes)==6','len(meshes)==10')
source=source.replace("'bezel-seam-after.json'","'connector-access-after.json','mic-design.json','mic-channel-check.json','np2-mic-removable.png','np2-duct-halves.png'")
exec(compile(source,str(path),'exec'))

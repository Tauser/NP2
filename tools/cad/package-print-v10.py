from pathlib import Path
path=Path('D:/Projetos/NP2/tools/cad/package-print-v06.py')
source=path.read_text(encoding='utf8').replace('v06','v10').replace('V06','V10')
source=source.replace("'bezel-seam-after.json'","'connector-access-after.json','mic-design.json','mic-channel-check.json','front-assembly-check.json','np2-mic-ducts.png','np2-mic-section.png','np2-underside.png'")
exec(compile(source,str(path),'exec'))

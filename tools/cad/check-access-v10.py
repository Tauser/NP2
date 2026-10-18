"""USB/SD required; internal plug envelopes pending dimensions, no pockets."""
def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/check-access-v07.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('Mesa V07','Mesa V10').replace('concept-v07','concept-v10')
 source=source.replace("('IO right / mating reference 18 x 7', [[36.825,-64,-14.65],[54.825,-46.8,-7.65]])","('IO right / oversized reference plug 32 x 7 x 6.2', [[23,-53,-14.65],[55,-46.8,-7.65]])")
 source=source.replace("('IO left / mating reference 18 x 7', [[-40.175,-64,-14.65],[-22.175,-46.8,-7.65]])","('IO left / oversized reference plug 32 x 7 x 6.2', [[-55,-53,-14.65],[-23,-46.8,-7.65]])")
 source=source.replace("('microSD / card and push clearance', [[-8.8,-65,-11.4],[4.5,-34,-9.55]])","('microSD / card corridor', [[-8.8,-76,-11.4],[4.5,-34,-9.55]])")
 source=source.replace("if d.attributes.itemByName('NP2','table_clearance_v07'):","if False:")
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

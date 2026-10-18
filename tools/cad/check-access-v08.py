def run(_context: str):
    path='D:/Projetos/NP2/tools/cad/check-access-v07.py'
    with open(path,encoding='utf8') as f:source=f.read()
    source=source.replace('Mesa V07','Mesa V08').replace('concept-v07','concept-v08')
    source=source.replace('internal_io=bool(d.attributes.itemByName(\'NP2\',\'internal_io_v07\'))','internal_io=False')
    source=source.replace("('IO right / mating reference 18 x 7', [[36.825,-64,-14.65],[54.825,-46.8,-7.65]])","('IO right / internal plug reference 32 x 7 x 6.2', [[23,-53,-14.65],[55,-46.8,-7.65]])")
    source=source.replace("('IO left / mating reference 18 x 7', [[-40.175,-64,-14.65],[-22.175,-46.8,-7.65]])","('IO left / internal plug reference 32 x 7 x 6.2', [[-55,-53,-14.65],[-23,-46.8,-7.65]])")
    source=source.replace("('microSD / card and push clearance', [[-8.8,-65,-11.4],[4.5,-34,-9.55]])","('microSD / card corridor', [[-8.8,-76,-11.4],[4.5,-34,-9.55]]),\n      ('IO right / wire bend reference', [[23,-53,-22.65],[55,-50,-14.55]]),\n      ('IO left / wire bend reference', [[-55,-53,-22.65],[-23,-50,-14.55]])")
    source=source.replace("if d.attributes.itemByName('NP2','table_clearance_v07'):","if False:")
    namespace={};exec(compile(source,path,'exec'),namespace);namespace['run'](_context)

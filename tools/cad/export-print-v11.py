def run(_context: str):
 path='D:/Projetos/NP2/tools/cad/export-print-v06.py'
 with open(path,encoding='utf8') as f:source=f.read()
 source=source.replace('V06','V11').replace('v06','v11')
 source=source.replace("('03 /','03-tampa-berco.stl',(8,130))","('03 /','03-tampa-berco.stl',(8,150))")
 source=source.replace("('08 /','06-cursor-onoff.stl',(276,222))]","('08 /','06-cursor-onoff.stl',(276,222)),('09 /','duto',(202,270))]")
 source=source.replace("elif prefix=='06 /':mgr.transform", "elif prefix=='09 /':mgr.transform(temp,rotation((0,1,0),-math.pi/2 if ' A /' in body.name else math.pi/2))\n                elif prefix=='06 /':mgr.transform")
 source=source.replace("parts.append((name,temp,pos))", "if prefix=='09 /':\n                name=['07-duto-mic-esquerdo-a.stl','08-duto-mic-esquerdo-b.stl','09-duto-mic-direito-a.stl','10-duto-mic-direito-b.stl'][idx];pos=(202+idx*36,270)\n            parts.append((name,temp,pos))")
 source=source.replace('SIX ORIENTED STL FILES','TEN ORIENTED STL FILES; FOUR OPEN DUCT HALVES')
 ns={};exec(compile(source,path,'exec'),ns);ns['run'](_context)

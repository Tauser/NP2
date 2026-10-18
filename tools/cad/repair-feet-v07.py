"""Reduce the foot top without changing the assembled 15 mm clearance."""
import adsk.core, adsk.fusion, importlib.util
def run(_context: str):
    d=adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    assert d.attributes.itemByName('NP2','table_clearance_v07')
    comp=next(o.component for o in d.rootComponent.occurrences if o.component.name.startswith('02 /'))
    for label in ['direito','esquerdo']:
        f=comp.features.extrudeFeatures.itemByName('Apoio '+label+' integrado / 15 mm livres sob corpo')
        assert f and f.deleteMe()
    d.timeline.moveToEnd()
    spec=importlib.util.spec_from_file_location('feet','D:/Projetos/NP2/tools/cad/raise-clearance-v07.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    module.add_feet(d)

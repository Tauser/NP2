"""Validate oriented binary STLs and package a geometry-only 350 mm plate."""
from pathlib import Path
import json, struct, zipfile, xml.etree.ElementTree as ET
import numpy as np

OUT=Path('D:/Projetos/NP2/mechanical/concept-v06')
manifest=json.loads((OUT/'print-manifest.json').read_text(encoding='utf8'))
ns='http://schemas.microsoft.com/3dmanufacturing/core/2015/02'
def tag(name):return '{'+ns+'}'+name
reports=[]
for idx,part in enumerate(manifest['parts'],1):
    path=OUT/'STL'/part['filename'];raw=path.read_bytes();n=struct.unpack_from('<I',raw,80)[0]
    assert len(raw)==84+50*n,path
    records=np.frombuffer(raw,offset=84,count=n,dtype=np.dtype([('normal','<f4',(3,)),('vertices','<f4',(3,3)),('attr','<u2')]))
    triangles=records['vertices'].astype(np.float64)
    vertices,faces=np.unique(np.round(triangles.reshape(-1,3),5),axis=0,return_inverse=True);faces=faces.reshape(-1,3)
    edges=np.sort(np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]),axis=1)
    _,counts=np.unique(edges,axis=0,return_counts=True)
    bad_edges=int(np.count_nonzero(counts!=2))
    cross=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);areas=np.linalg.norm(cross,axis=1)/2
    volume=float(np.sum(np.einsum('ij,ij->i',triangles[:,0],np.cross(triangles[:,1],triangles[:,2])))/6)
    normals=cross/np.maximum(2*areas[:,None],1e-20)
    mins=vertices.min(axis=0);maxs=vertices.max(axis=0);size=maxs-mins
    assert np.allclose(size,part['size_mm'],atol=.001),part['filename']+' unit mismatch'
    assert abs(mins[2])<.0001 and volume>0 and bad_edges==0,part['filename']+' mesh invalid'
    bed=(triangles[:,:,2].max(axis=1)<.0001)&(normals[:,2]<-.99)
    contact=float(np.sum(areas[bed]));assert contact>1,part['filename']+' no flat bed face'
    steep=(normals[:,2]<-.72)&(triangles[:,:,2].min(axis=1)>.21)
    report={'filename':part['filename'],'triangles':n,'watertight_edge_incidence':bad_edges==0,'nonmanifold_or_open_edges':bad_edges,'volume_mm3':volume,'size_mm':size.tolist(),'bed_contact_area_mm2':contact,'downward_surfaces_beyond_approx_45_deg_mm2':float(np.sum(areas[steep]))}
    reports.append(report)
with zipfile.ZipFile(OUT/'NP2-v06-ABS-350x350.3mf') as z:
    assert z.testzip() is None
    model=ET.fromstring(z.read(next(n for n in z.namelist() if n.lower().endswith('.model'))))
    assert model.attrib.get('unit','millimeter')=='millimeter'
    objects={obj.attrib['id']:obj for obj in model.find(tag('resources')).findall(tag('object'))}
    def transform(text):
        values=np.array([float(v) for v in text.split()]) if text else np.array([1,0,0,0,1,0,0,0,1,0,0,0])
        return values[:9].reshape(3,3),values[9:]
    meshes=[]
    def visit(objectid,parent_matrix,parent_translation):
        obj=objects[objectid];mesh=obj.find(tag('mesh'))
        if mesh is not None:
            points=np.array([[float(v.attrib[k]) for k in ['x','y','z']] for v in mesh.find(tag('vertices'))])
            points=points@parent_matrix+parent_translation
            meshes.append({'name':obj.attrib.get('name',''),'min_mm':points.min(axis=0).tolist(),'max_mm':points.max(axis=0).tolist()})
        comps=obj.find(tag('components'))
        if comps is not None:
            for child in comps:
                matrix,translation=transform(child.attrib.get('transform',''))
                visit(child.attrib['objectid'],matrix@parent_matrix,translation@parent_matrix+parent_translation)
    for item in model.find(tag('build')):
        matrix,translation=transform(item.attrib.get('transform',''));visit(item.attrib['objectid'],matrix,translation)
    assert len(meshes)==6,meshes
    for mesh in meshes:
        lo,hi=np.array(mesh['min_mm']),np.array(mesh['max_mm'])
        assert abs(lo[2])<.001 and np.all(lo[:2]>=0) and np.all(hi[:2]<=350),mesh
    for i,mesh in enumerate(meshes):
        lo=np.array(mesh['min_mm'])[:2]-8;hi=np.array(mesh['max_mm'])[:2]+8
        assert np.all(lo>=-.001) and np.all(hi<=350)
        for other in meshes[i+1:]:
            other_lo=np.array(other['min_mm'])[:2]-8;other_hi=np.array(other['max_mm'])[:2]+8
            assert not np.all((lo<other_hi)&(other_lo<hi)),(mesh['name'],other['name'])
data={'method':'mesh topology, bounds, flat contact and downward surface screening; not a printer-specific slice','nozzle_assumed_mm':.4,'layer_height_reference_mm':.2,'parts':reports,'native_3mf_plate_check':meshes,'plate_bbox_clearance_including_brim_mm':8}
(OUT/'mesh-check.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(data,ensure_ascii=False,indent=2))
archive=OUT/'NP2-v06-ABS-impressao.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for path in sorted((OUT/'STL').glob('*.stl')):z.write(path,'STL/'+path.name)
    for name in ['NP2-v06-ABS-350x350.3mf','GUIA-IMPRESSAO.md','README.md','print-manifest.json','mesh-check.json','fit-check.json','bezel-seam-after.json','np2-print-layout.png']:z.write(OUT/name,name)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
print('PRINT PACKAGE',archive)

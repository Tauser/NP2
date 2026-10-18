from pathlib import Path
import struct, numpy as np
for path in Path('D:/Projetos/NP2/mechanical/concept-v06/STL').glob('*.stl'):
    data=path.read_bytes();n=struct.unpack_from('<I',data,80)[0]
    rec=np.frombuffer(data,offset=84,count=n,dtype=np.dtype([('normal','<f4',(3,)),('vertices','<f4',(3,3)),('attr','<u2')]))
    t=rec['vertices'].astype(float);volume=np.sum(np.einsum('ij,ij->i',t[:,0],np.cross(t[:,1],t[:,2])))/6
    cross=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]);area=np.linalg.norm(cross,axis=1)/2
    print(path.name,n,'vol',volume,'minz',t[:,:,2].min(),'minarea',area.min(),'zero',np.count_nonzero(area<1e-10))
    for precision in [None,6,5,4]:
        coords=t.reshape(-1,3) if precision is None else np.round(t.reshape(-1,3),precision)
        verts,ids=np.unique(coords,axis=0,return_inverse=True);faces=ids.reshape(-1,3)
        deg=(faces[:,0]==faces[:,1])|(faces[:,1]==faces[:,2])|(faces[:,2]==faces[:,0])
        faces=faces[~deg]
        edges=np.sort(np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]]),axis=1)
        unique,counts=np.unique(edges,axis=0,return_counts=True)
        print('precision',precision,'deg',deg.sum(),'edge_counts',dict(zip(*np.unique(counts,return_counts=True))))
        if precision is None:
            for edge,count in zip(unique[counts!=2][:12],counts[counts!=2][:12]):print('BAD',count,verts[edge].tolist())

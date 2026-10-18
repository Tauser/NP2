# Referencia mecanica oficial da Waveshare 7B

Fonte: https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents

Pacote: https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B-3D.zip

Obtido em 2026-10-09. STEP e desenho no pacote datados de 2025-11-08.
SHA256 de `official-7b.stp`: F2104DC257EE8575459DDF74404E32FAF81E4BCDA23A8BFEC23B6604C089986C.

O desenho especifica PCB de 1,6 mm e espessura maxima do conjunto de 15,3 mm.
A PCB tem 164 x 97 mm; distancia entre bases M2.5 de 156 x 89 mm.
O vidro mede 164,28 x 99,17 mm, com area ativa de 154,58 x 86,42 mm.

No STEP, o vidro frontal esta em Z=-8,8 mm; a face traseira da PCB em Z=0;
as extremidades das bases originais em Z=6 mm. Portanto, as distancias do
vidro sao 8,8 mm e 14,8 mm, compativeis com as medidas arredondadas de 9 e
15 mm informadas pelo usuario. A geometria BOARD inclui faixas inferiores
ate Z=-3,1; isso nao significa PCB de 3,1 mm: os planos principais da PCB
estao em Z=-1,6 e Z=0, conforme o desenho.

Alinhamento ao case: rotacao de 180 graus sobre Y; translacao local
(82, -48, -9,45) mm. Isso coloca o vidro em Z=-0,65 mm e seu centro em
X=Y=0. Os USBs e a chave ficam a esquerda quando vistos pela frente.
O centro da area ativa fica em (-1,79, +2,355) mm, preservando a borda
maior do vidro a direita. Os centros das bases ficam em X=+-78 e
Y=-44 / +45 mm. Nao espelhar a geometria para conservar o lado antigo.

Os JSONs contem envelopes em coordenadas oficiais, medidos no Fusion.
Envelopes completos de conectores incluem terminais e abas; diferem das
dimensoes da boca para insercao do cabo. O encaixe de cabos e o curso da
chave exigem verificacao de montagem.

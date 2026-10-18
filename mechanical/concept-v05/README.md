# Case NP2 V05 — montagem com hardware oficial

Revisão de 2026-10-09, criada no Fusion 360 pelo MCP local.
Sem base separada e sem compartimento de bateria. Tela inclinada 18 graus
em relação à vertical, traseira plana vertical e corpo com cantos R6.
A borda plástica direita é menor para compensar a borda maior do vidro
original. A placa foi orientada pela frente real; USBs e ON/OFF ficam à
esquerda para quem olha para a tela.

## Arquivos

- `NP2-case-v05.f3d`: documento Fusion com histórico, componentes e referência oficial.
- `NP2-case-v05.step`: montagem para intercâmbio CAD, incluindo as referências de hardware.
- `np2-front.png`, `np2-rear.png`, `np2-inside.png`, `np2-usb-side.png`:
  vistas da montagem em posição de uso.
- `np2-cover-cradle.png`: interior da tampa com berço e quatro travas.
- `fit-check.json`: evidência da verificação geométrica.

## Dimensões e acessos

A moldura mede 174,28 × 110 mm no seu próprio plano. Em posição de uso,
o conjunto mede aproximadamente 175,88 mm de largura, incluindo o cursor
ON/OFF, 60,10 mm de profundidade e 104,49 mm de altura. A tampa tem
170,28 × 96,87 mm. A parede nominal do corpo é 2,4 mm, com acabamento CAD
de 0,03 mm; a tampa tem 2,4 mm.

O painel USB foi rebaixado até X=-83,7 mm, com parede local de 1,2 mm.
Recuo da boca metálica para a superfície rebaixada: USB-A 1,70 mm;
USB-C e USB UART 2,02 mm. As aberturas são 19 × 11 mm para o USB-A e
13,5 × 8,5 mm para cada USB-C, permitindo a entrada da capa dos plugs.
O espaçamento e as profundidades foram medidos no STEP oficial.

Os dois botões de volume estão no topo, com suporte para tact switches
separados. O cursor lateral acopla a lingueta ON/OFF original, com folga
geométrica de aproximadamente 0,2 mm por lado no encaixe da lingueta.

## Alto-falante sem parafusos

Dimensão informada pelo usuário: 100 × 45 × 20 mm. O berço foi incorporado
diretamente à tampa; ambos e as quatro travas formam um único sólido conectado.
Não há abas ou furos de parafuso para montar o alto-falante ou o berço.
Os quatro parafusos restantes da tampa permitem abrir o próprio case.

O alojamento interno mede 104 × 49 mm. O alto-falante repousa no assento
periférico a 5 mm da face interna da tampa, reservando espaço para uma
junta acústica. As quatro linguetas têm fendas laterais de 0,8 mm e cerca
de 15 mm de comprimento livre. Cada lábio cobre a borda do envelope em
0,4 mm, deixa 0,35 mm atrás do alto-falante e possui rampa de entrada.
O alto-falante é pressionado no alojamento e pode ser liberado afastando
as linguetas. A grelha traseira fica diretamente atrás dele.

## Referência e validação

Fonte oficial e datums em `../reference/waveshare-7b/README.md`.
STEP Waveshare de 2025-11-08; SHA256:
`F2104DC257EE8575459DDF74404E32FAF81E4BCDA23A8BFEC23B6604C089986C`.
As medidas de 9 mm do vidro à placa e 15 mm às bases informadas pelo usuário
são compatíveis com 8,8 e 14,8 mm do modelo oficial. As bases M2.5 originais
foram preservadas; os apoios do case encostam nas extremidades dessas bases.

O verificador usa cópias BRep transformadas ao plano local do display;
não usa caixas sobreestimadas de ocorrências rotacionadas como geometria.
Compara as peças do case entre si e com os 1.345 corpos de hardware,
sem testar interferências internas da própria montagem do fabricante.
Seis corpos de superfície do soquete microSD e suporte RTC são substituídos
por envelopes conservadores. O filtro de região central vazia é aplicado
somente à casca do corpo, não aos demais componentes.

Resultado: 413 interseções booleanas verificadas, zero colisões acima de
0,01 mm³, zero falhas booleanas, nenhum erro ou aviso de feature.
Cada peça fabricável possui um único volume conectado. O grupo de histórico
importado `Grupo1` apresenta aviso sem mensagem; está registrado separadamente
no relatório e não corresponde a erro geométrico de uma feature.

## Pendências de montagem física

O encaixe de capas reais de cabos e a elasticidade/força das travas dependem
das tolerâncias e do material da impressão. Conferir com uma impressão de teste.
O curso completo da chave ON/OFF e a escolha/ligação dos tact switches de volume
ainda exigem teste com os componentes físicos. A referência do alto-falante é
um envelope dimensional, não um STEP confirmado do alto-falante.
Os bocais frontais dos microfones estão modelados; o caminho acústico vedado
até os microfones da placa permanece a definir. O bocal direito foi deslocado
para evitar o flex do touch, cujo alívio oculto preserva 0,7 mm de pele frontal.

## Reprodução

Executar `tools/cad/create-case-v05.py` via `tools/cad/invoke-fusion.ps1` com
timeout suficiente para importar o STEP (por exemplo, 1.200 segundos).
O script cria um documento novo, realiza as revisões e valida antes de exportar.
Documentos e artefatos das revisões anteriores são preservados.
Nenhum firmware foi alterado.

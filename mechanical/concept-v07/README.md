# NP2 V07 — USB e acesso ao microSD

Revisão de 2026-10-10 para ABS e mesa de 350 × 350 mm. A V06 foi preservada.

O rebaixo lateral dos USBs foi ampliado para 58 × 18 mm. O rebaixo comum
libera as capas dos plugs, enquanto as bocas individuais continuam
alinhadas aos conectores oficiais. Foram conferidos corredores retos
de inserção para capas de referência USB A de 22 × 13 mm e USB C/UART
de 14 × 10 mm, além das pontas metálicas. São envelopes de projeto;
os cabos reais do usuário ainda não foram medidos.

Foi criada uma abertura inferior de 22 × 12 mm, com quinas arredondadas,
alinhada ao microSD e com espaço para pressionar o cartão. A inserção e
remoção exigem levantar ou inclinar o aparelho: a mesa bloqueia o
corredor de serviço quando ele está apoiado.

Os dois conectores inferiores foram mantidos internos, conforme a
preferência do usuário por componentes adicionais opcionais. A montagem
é acessível pela tampa. O encaixe dos futuros plugs e o percurso dos fios
não estão aprovados: é necessário medir os cabos/componentes escolhidos
e dimensionar os bolsos internos. Os corredores longos de conexão externa
colidem com a parede inferior; esse resultado continua registrado, sem
ser usado como validação de uma conexão interna ainda indefinida.

Há ainda uma divergência de referência: a documentação da
[Waveshare identifica os conectores inferiores como PH2.0 de 12 pinos](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B),
enquanto os componentes correspondentes do STEP se chamam
`PITCH1MM-12PIN-SMT-HORIZONTAL` e medem 14,35 mm de largura.
Não usar esses componentes simplificados como molde final dos plugs reais.

O case mantém a altura da V06, sem os apoios adicionais estudados durante
esta revisão. Moldura corrigida, berço do alto-falante, tampa, microfones,
botões e demais encaixes foram preservados. O corpo continua uma peça.

## Arquivos

- `NP2-case-v07.f3d` e `.step`: montagem com o hardware oficial.
- `NP2-v07-pecas-impressao.f3d`: seis peças orientadas para imprimir.
- `NP2-v07-ABS-impressao.zip`: STLs, placa 3MF, guia e relatórios.
- `connector-access-after.json`: corredores USB/microSD e pendências do IO interno.
- `fit-check.json`: encaixes entre peças e STEP oficial.
- `mesh-check.json`: malhas, contato com a mesa e distribuição no 3MF.

Os encaixes CAD da placa e das peças foram novamente conferidos, sem
colisões acima de 0,01 mm³. Essa verificação não inclui plugs internos
reais ainda não definidos. Não houve impressão física ou teste com cabos.

## Reprodução

Ativar a V06 corrigida e executar `tools/cad/create-case-v07.py` pelo MCP
do Fusion. Depois executar `tools/cad/package-print-v07.py` com o Python
do runtime para validar as malhas e produzir o ZIP.

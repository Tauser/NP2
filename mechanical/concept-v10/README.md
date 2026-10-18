# NP2 V10 — dutos dos microfones e fundo liso

Revisão de 2026-10-10. Mantida a extensão inferior de 4 mm da V09.
Os rebaixos dos plugs foram removidos, restaurando o fundo liso com parede
nominal de 2,4 mm. Corpo, moldura e tampa mantêm as dimensões externas.

## Microfones

O [modelo oficial Waveshare](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents)
posiciona as cápsulas em X = −68/+68 mm e Y = −45,39999 mm nas coordenadas
do painel. O furo direito anterior estava em X = +76 mm, deslocado 8 mm.
As aberturas antigas foram fechadas e os pequenos bocais anteriores removidos.

As novas entradas de 1,6 mm ficam em X = −68/+68 mm e Y = −53,5 mm, na mesma
linha da borda inferior. Esse afastamento do vidro permite contornar o
conjunto de touch/LCD sem atravessá-lo. Dois dutos independentes estão
integrados ao corpo: canal de 2 mm, curva inferior de 2 × 2 mm e câmara
de 4,2 × 3,4 mm em torno de cada cápsula. Os percursos têm o mesmo comprimento
geométrico de aproximadamente 23,43 mm, conforme `mic-channel-check.json`.

O STEP contém o envelope de cada cápsula de 3,4 × 2,6 × 0,998 mm, mas não
modela a abertura acústica pequena. Por isso a câmara abrange a face inteira,
sem supor a posição exata do pequeno orifício. Conferir na placa física
que a entrada é voltada para essa câmara antes de fabricar as juntas.

## Vedação obrigatória

Os dutos de ABS sozinhos não garantem vedação acústica. São necessárias:

| Local | Quantidade | Dimensões da junta macia | Espessura livre / na montagem |
| --- | ---: | --- | --- |
| Entre entrada do duto e moldura | 2 | Diâmetro externo 4 mm; interno 2 mm | 0,5 / 0,43 mm |
| Entre câmara e PCB, em torno do microfone | 2 | Exterior 6,6 × 5,8 mm; janela 4,2 × 3,4 mm | 2 / 1,65 mm |

Usar espuma macia de células fechadas apropriada para vedação, com corte
limpo; escolher o material pela compressão real e recuperação. Não imprimir
essas juntas em ABS. As referências CAD mostram o estado comprimido.
Não obstruir o pequeno orifício do microfone com adesivo, espuma ou sujeira.
A junta traseira apoia na PCB ao redor da cápsula, evitando carga direta
sobre ela. O conjunto deve entrar pela frente do corpo com a moldura removida.

`np2-mic-section.png` é um corte real do CAD em X = +68 mm. Azul: corpo/duto;
cinza superior: moldura; verde: PCB; cinza sob a PCB: microfone;
laranja: juntas comprimidas. A imagem não representa simulação de som.
`NP2-v10-corte-duto-revisao.f3d` é apenas documento de revisão, não peça de impressão.

## Validação e limites

Verificações: encaixe com os 1.345 corpos oficiais, continuidade dos sólidos,
canal desobstruído por sondas geométricas e paredes nos trechos fora das
juntas de passagem. A entrada pela frente foi amostrada em cinco posições
para PCB, LCD, microfones e quatro bases, com a moldura removida; não é
uma simulação completa de montagem manual ou tolerâncias.
Não foram medidos ganho, resposta em frequência,
isolação do alto-falante, desempenho de AEC ou força de compressão das juntas.
Montar um protótipo e testar cada microfone e ambos juntos, com o alto-falante
desligado e ligado, antes de considerar o áudio finalizado.

USBs e microSD preservam os acessos da V09. A utilização dos dois conectores
inferiores continua opcional e depende dos plugs reais: os envelopes grandes
de 32 × 7 × 6,2 mm usados na V09 encostam no fundo liso e não estão aprovados
para esta revisão. O relatório registra essa limitação sem alterar o corpo.

Seis peças de ABS no pacote: moldura, corpo com dutos, tampa com berço,
duas teclas de volume e cursor ON/OFF. Mesa de 350 × 350 mm; STL em mm;
3MF com orientação e geometria, sem perfil de impressora. V09 preservada.

## Reprodução

Pelo MCP do Fusion: `new-v10.py`, `remove-reliefs-v10.py`, `mic-ducts-v10.py`,
`verify-v10.py`, `check-mic-channels-v10.py`, `check-access-v10.py`,
`check-front-assembly-v10.py`,
`render-mic-detail-v10.py`, `finish-v10.py`, `render-access-v10.py`,
`export-print-v10.py`. Depois executar `package-print-v10.py` com o Python
do runtime. As juntas macias são cortadas separadamente.

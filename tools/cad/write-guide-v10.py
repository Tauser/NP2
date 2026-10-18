from pathlib import Path
out=Path('D:/Projetos/NP2/mechanical/concept-v10')
source=Path('D:/Projetos/NP2/mechanical/concept-v09/GUIA-IMPRESSAO.md').read_text(encoding='utf8').replace('V09','V10').replace('v09','v10')
source=source.split('## Ajustes da V10')[0]
source=source.replace('Os tact switches de volume e os dutos acústicos vedados dos microfones\ncontinuam pendentes da definição dos componentes físicos.','Os tact switches de volume continuam pendentes da definição dos componentes\nfísicos. Os dutos estão modelados; vedação e resposta acústica exigem teste real.')
source+='''## Ajustes da V10

Os rebaixos dos plugs internos foram removidos e o fundo ficou liso. Mantida
a extensão de 4 mm. Os plugs grandes da referência anterior não estão
aprovados nesta versão; comparar os cabos reais antes de montar opcionais.

O corpo agora inclui os dois dutos MIC, com canal de 2 mm e câmaras sobre
os microfones. Há duas juntas frontais macias de 0,5 mm e duas juntas
traseiras de 2 mm, cortadas separadamente conforme a tabela do README.
As referências de juntas não entram nos seis STLs de ABS.

Mantenha os canais acústicos livres de suporte. Confira na prévia as pontes
curtas de 2 mm e paredes das curvas. Use suporte externo localizado apenas
onde o fatiador indicar; não deixe suporte preso dentro do canal. Caso seu
perfil não consiga fazer as pontes curtas, testar o trecho do duto antes de
imprimir o corpo inteiro. Não foram executados fatiamento nem impressão real.

Após imprimir, verificar passagem de luz/ar pelos dois canais e ausência
de detritos sem usar pressão excessiva nos microfones. Instalar as juntas
limpas antes da placa e da moldura, mantendo os orifícios livres de adesivo.
Inserir o conjunto de placa e tela pela frente, com a moldura removida.
Não pressionar diretamente a cápsula do microfone com o duto rígido.

A câmara foi desenhada em torno do envelope completo; confirmar o orifício
acústico real e a compressão da espuma no protótipo. A qualidade do áudio
não foi validada por estes testes CAD.
'''
(out/'GUIA-IMPRESSAO.md').write_text(source,encoding='utf8')

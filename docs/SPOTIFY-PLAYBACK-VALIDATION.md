# Reprodução Spotify — laboratório P4, 23/10/2026

> Histórico encerrado em 24/10/2026 por solicitação do responsável. O produto
> passou a usar [controle remoto na Home](SPOTIFY-REMOTE.md); estes ensaios
> nativos não compõem a imagem atual nem provaram reprodução musical.

**Resultado atual:** bloqueado na chave de áudio. AP aceitou conta/metadata,
mas negou a chave em96kbps e em160kbps, neste último caso código00 01 em
duas faixas distintas. Nenhuma música foi reproduzida. As seções seguintes
registram os incrementos e ensaios; seus resultados intermediários não
constituem integração concluída. Sessão de teste encerrada ao fim.

O pareamento anterior passou até autenticação de conta, após seleção pelo
operador no celular. Esta variante acrescenta reprodução e precisa de nova
seleção: credenciais de conta ficam apenas na RAM e não sobrevivem ao reboot.
Até a confirmação física abaixo, não há evidência de música reproduzida.

## Imagem e reprodução do ensaio

```powershell
tools/run_spotify_pairing_bench.ps1 -Playback -BuildDirectory firmware/build/spotify-playback-20261023 -Flash -Port COM8
C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe tools/capture_agenda_diagnostics.py --port COM8 --seconds 75 --reset --output firmware/build/spotify-playback-20261023/reset-boot-sanitized.log
C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe tools/pair_spotify_panel.py --port COM8 --playback --seconds 600 --output firmware/build/spotify-playback-20261023/playback-sanitized.log
```

App-flash verificado, somente P4 em 0x20000, sem alterar NVS/partições/C6.
Primeiro bin SHA-256: `5e00042f154bd1e2521fb8e8ad36288c5353540020c690c4ba52762199cbccf1`.
Correção resolve/token4KiB compilada e app-flash verificado:
`12166975cda0134e00da52841f4b8ae0d90c2ca8c857eb06436f898ba9e6f016`.
Build IDF 5.5.4/esp32p4, ambos os flags de laboratório ON, probe de áudio
sintético OFF. Exceções C++/pool 2 KiB e XiP/stack PSRAM exigidos pelo perfil.
SRAM estática 70.880 B, BSS externa 136.336 B; verificador de placement passou.
Pins e versões permanecem os do ensaio de pareamento.

O helper lê somente o arquivo local ignorado; o operador libera a USB em
Preferências → Sistema → Parear Spotify. Antes de anunciar o receiver, o P4
decodifica um fixture público próprio de 3 s, sem reproduzi-lo. A prova deve
indicar 529.200 B PCM estéreo, 66.150 amostras mono, integridade 1 e resultado 0.
Somente depois da fase 4, selecionar NovaPanel teste XXXX no Spotify e tocar
uma faixa. Fases 6/9 provam sessão; PCM consumido prova entrega ao owner;
som audível requer confirmação do operador. O controle permanece no celular.

## Limites e verificações

Decoder Tremor privado com ledger de 2.048 alocações, teto de 2 MiB em PSRAM,
recuperação C de OOM e limpeza integral. Formato aceito 44,1 kHz estéreo;
FIR converte para PCM16 mono 22,05 kHz do owner ES8311 existente. Fila com
backpressure, geração e marcadores de faixa só avançam quando consumidos.
Ganhos iniciais Spotify 20%; preferência geral de volume preservada.

HTTP usa o executor único, ranges até 8 KiB e o piso de SRAM interna existente;
TLS/JSON continuam internos. Pilha privada Spotify 48 KiB em PSRAM, HTTPD
8 KiB. HTTPD/anúncio próprios encerram antes do streaming. Uma tentativa por
boot, janela de pareamento 5 min, sem reconexão automática ou persistência.
Sem PCM inicial por 300 s ou pausa por 10 min encerra o ensaio. A espera
de carregamento por faixa é de até60s, interrompível por stop/reset a cada100ms.

Host: fixture Vorbis de 529.200 B; 103 pontos de falha de alocação com limpeza,
read fragmentado/seek/formato inválido/truncamento. Comparação ffmpeg:
RMSE 0,706278, SNR 55,285 dB. Pico do codec host 31.996 B, 100 blocos.
Testes de sink verificam saída exata sob escritas parciais/retry e marcadores.
Pacotes incluem ACK Mercury com header vazio válido, MAC, limites e mutações.
Continuam warnings externos e ausência de qualificação de todo input malformado.

## Evidência física desta imagem

Boot filtrado por 75 s: P4 rev103, PSRAM 32 MiB, SD com nove fotos,
preferências preservadas, sem fault registrado. Google token/Tasks/Calendar
200, resultado 0, mínimo interno 110.895 B, após 165.259 B, maior 83.968 B,
stack rede 4.148 B, app_loop 2.236 B, integridade 1. Clima, BCB,
Alternative.me e três brapi 200. C6 reportou 3.0.6; sem regravação nem nova
medição do hash instalado. Isso não prova estabilidade gráfica ou streaming.

Após liberação física, app validado87,6s. Fixture no P4 passou88,2s:
resultado0, PCM529.200B, mono66.150amostras, pico do codec31.356B/100blocos,
pico de amostra611, 5.987cruzamentos, stack mínima38.808B, integridade1.
AP autenticado89,4s e receiver anunciado89,5s, interno160.807B,
maior83.968B. Tempos relativos ao início do helper, incluindo espera física.

Música, throughput, pausas/trocas e concorrência de alarmes aguardam ensaio.
Não fecha ADR-103, G3/G6 nem integração de produto.

Primeira seleção nesta imagem: blob recebido113,1s, interno165.343B,
stack38.676B, integridade1. O segundo resolve AP foi recusado antes de TLS:
interno154.619B, resultado257/ESP_ERR_NO_MEM, sem PCM. Não houve crash.
O limite genérico de resposta8KiB exigia155.648B disponíveis. Correção:
resolve/token passam a ter teto4KiB (respostas acima são recusadas), mantendo
o piso80KiB e envelopeTLS64KiB. Storage conserva8KiB. Testes host de política
e ponte passaram com limites novos. A imagem corrigida requer novo ensaio.

Segunda tentativa (bin12166975…): app/fixture passaram, mas conexão AP
encerrou antes da fase3 após resolve HTTPS ESP_OK. Sem anúncio/PCM, sem fault
de runtime, integridade1/stack38.796B. A imagem seguinte adiciona diagnóstico
numérico de etapa/motivo, comparando apenas mensagens constantes conhecidas;
texto de exceção externo nunca é exibido. Somente erro de socket no preflight
permite até três tentativas, separadas por500ms, cada uma com seus prazos
limitados existentes. Falhas de assinatura/parser não são retentadas; nenhuma
reconexão de conta/streaming foi habilitada.

Imagem de diagnóstico compilada e app-flash verificado:
bin `8b907e332ad0f99b950f50dc70484928a57761ee88ecaf3933bb1e92e4db930a`;
ELF `41a4ac4caa1d5ba67ca97eb22a27213227bb26fe31727445d9115858907652f3`.
As imagens anteriores acima são históricas. Config/partições inalteradas;
NVS/C6 preservados. Helper passou quatro testes host, incluindo filtragem.
`failure stage`:3=preflight AP;10=conexão AP da conta;11=autenticação;
12=inicialização dos workers/país;13=sessão ativa. `reason`:0=outro erro
omitido;1=socket;2=resposta inválida;3=memória do parser;4=assinatura AP;
5=autenticação inválida;6=protobuf;7=pacote;8=país ausente;9=workers;
10=conta recusada. Esse diagnóstico não imprime texto de exceções.

Terceira tentativa: janela do helper PC havia expirado antes da resposta do
operador; nenhum aceite. Helper corrigido para manter envio privado a cada2s
até seu prazo total (máximo600s), encerrando envios no aceite. A liberação
física60s continua obrigatória e inalterada no dispositivo.

Após nova liberação: app/fixture/AP passaram; blob310,7s; conta/sessão ativa
311,9s (fases6/9). ResolveAP200/169B/497ms, interno antes154.255B,
mínimo128.419B, rede4.216B. Sessão interna145.387B, maior83.968B, worker38.676B,
integridade1. Token de mídia recusado antesTLS:142.799B inicialmente,
130.479B após lista recebida. SemPCM em120s, encerramento controladofase8.

Próxima imagem move containers URI/context de listas limitadas128faixas e
bufferPCM1KiB para PSRAM, usando allocator privado. Nenhuma mudança de TLS,
JSON, credenciais, piso/envelope ou política de heap global. Codificação de
texto usa callback com o tipo correto; teste host faz roundtripprotobuf real
de20referências URI/context/GID/queued e URI→GID. Testes CDN/helper passaram.
Pico/gain de heap dessa mudança ainda requer medição no P4.

Imagem PSRAM de mídia compilada e app-flash verificado,4.104.720B:
bin `9528448dbdc7d52db907b59583ba1ae69c28ec35902b8dc5f1584b2cb564e57c`;
ELF `d6425d76673624e8c1155e3b5e36e2af5f550b9c65733df2049b238df3e4d946`.
Config/partições conservam hashes anteriores. Manifesto público e hashes
no diretório ignorado do build; base777abde7069c91cfb6aeb915891cf00bc249f9df,
árvore com alterações anteriores preservadas, sem commit de entrega.

Boot da imagem PSRAM de mídia60s sem fault registrado: Google200/token,
tasks/calendar; resultado0, min111.131B/after165.355B, maior96.256B,
rede4.212B/app_loop2.232B/integridade1. SD9fotos, preferências preservadas,
clima/BCB/Alternative.me/trêsbrapi200. Placement70.880B/136.336B passou.

Primeira espera PC600s dessa imagem terminou sem aceite observado. Após o
operador confirmar o toque, helper retomado aproveitando a janela física:
app0,8s, fixture1,4s, AP2,5s, anúncio2,6s. Todos resultado0/integridade1;
interno160.847B/maior96.256B/worker38.808B na fase4. Não confundir timeout
do helper antes do aceite com falha de anúncio ou de conta no dispositivo.

Ensaio seguinte da mesma imagem: receiver selecionado no celular, conta e
Connect ativos53,6s. Operador viu estado “tocando” no aplicativo, mas PCM
permaneceu0. Token bloqueado antesTLS, inicialmente142.723B, depois145.683B/
147.555B; sessão interna estabilizou151.647–153.999B, contra136.487B do ensaio
anterior (listas/textos PSRAM). Condições de rede/playlist diferentes impedem
atribuir toda a diferença ao allocator. Encerramento120s semPCM, integridade1.
Prova de pareamento/estado remoto, não de música audível.

Nova correção: job HTTP contíguo interno com tamanho exato dos argumentos
limitados, evitando máximos de URL/auth em todo request. Host verifica job
resolve menor que1KiB, ownership/refs/prazos/limpeza. GIDs públicos de faixa
também em PSRAM; callbacks próprios de vetor no protobuf, cópia temporária
de16B para helpers de URI/hex. Roundtrip de referências e CDN passaram.
Piso80KiB/envelopeTLS64KiB/ranges8KiB permanecem inalterados.

Imagem com job exato/GIDs PSRAM compilada e app-flash verificado,4.106.240B:
bin `4b8bcb47648c2a044207eb70840d7a56167b3c58376edf96804f9599a34f5cbf`;
ELF `d97ac55e2ddb95c587e797d83fc45c347a7e92a8e18172762b1a4fe09519a75a`.
Placement70.880B internos/136.336B externos passou; config/partições/C6/NVS
preservados. TrackPlayer/Vorbis/guard/fixture presentes; probePCM ausente.
Testes host de ponte, referências (allocator de tipo distinto no host) e CDN
passaram. Ainda falta a medição do request de mídia nessa imagem.

Ensaio da imagem job exato: app/fixture/AP passaram; blob114,5s. Resolve
seguinte200/169B/552ms, interno antes159.623B/min132.687B/rede4.156B.
Falha de socket na etapa10 antes da autenticação de conta, em115,4s;
encerramento controlado/integridade1/worker38.676B, semPCM. Diferente do
bloqueio de SRAM anterior; nenhum download de mídia foi iniciado nessa vez.

Próxima imagem detalha socket somente por operação/código numéricos:
1=select/prazo,2=endpoint,3=DNS,4=connect,5=tamanho de pacote,6=read,
7=EOF,8=write. Não imprime host/IP/endereço/URL/conta. Antes de autenticar,
socket permite até3tentativas com500ms; cada tentativa cria MercurySession
nova e repete DH/validação do servidor. Não reutilizar cipher/DH parcialmente
inicializados. Assinatura/parser continuam fail-closed. close() fica seguro
mesmo se a falha ocorreu antes de criar conn. Sem reconexão após autenticação.

Imagem de retry de socket compilada e app-flash verificado,4.106.848B:
bin `a38fd92441e00ba979f09dc51d235c0bef30cfc34449d06be5994082917860e3`;
ELF `e17159aab177528c6aac1f4a864f9130dddcd450399c3a98a9be1f26417a8304`.
Config/partições e C6/NVS preservados; placement70.880B/136.336B passou.
Helper4grupos e roundtrip de referências passaram. A recuperação de erro
e o download de mídia nessa imagem continuam pendentes de execução física.

Ensaio seguinte: conta/Connect67,7s, token200/199B/573ms após primeira
recusa de memória. Token antes152.411B/min126.927B/rede4.208B. Sem request
storage/CDN observado nem PCM até o prazo120s original; operador confirmou
que o app mostrava tocando, sem som. Sessão interna153.451–153.875B, maior
83.968B, worker38.676B, integridade1. Readcode128 ocorreu no fechamento
controlado; não é prova de queda espontânea da conexão.

Nova correção: prazo do player5s era incompatível com request20s e backoff30s;
agora espera até60s com checagens100ms, cancelando por stop/reset. Prazo sem
PCM do ensaio ampliado120→300s. Diagnóstico numérico de mídia:
stage1=tipo frame/quantidade faixas;2=estado da entrada;3=status Mercury/
partes;4=quantidade de arquivos/máscara de formatos;5=partes/bytes metadata;
6=sucesso/tamanho da resposta de chave;7=estado ao exceder espera;
8=falha de abertura do stream. Sem títulos/IDs/contas/URLs/chaves em logs.
Estado da entrada:0queued,1metadata,2precisa-chave,3aguarda-chave,4precisa-CDN,
5pronta,6falhou. Status sint32 campo4 vem do
[Header Mercury do librespot](https://raw.githubusercontent.com/librespot-org/librespot/dev/protocol/proto/mercury.proto).
Leitor diagnóstico limitado cobre status200/negativo, ausência, duplicata,
truncamentos e20mil mutações; não altera a decisão do parser existente.

Imagem de espera/diagnóstico de mídia compilada e app-flash verificado:
4.107.776B, bin `8ee2ae8b9f57244ccb7f686a42d99ff2f7833e1869f2a66b7447b4f974341e8a`;
ELF `d7491716dab9a64dc7dc4126f935546791d39f8841b4a45954010c8ad934881d`.
Config/partições/C6/NVS preservados, placement70.880B/136.336B passou.
Boot50s sem fault registrado, Google200/token/tasks/calendar, min111.103B/
after165.367B/maior96.256B/rede4.144B/app2.232B/integridade1; SD9fotos e
preferências restauradas, provedores habituais200. Pacotes/status, referências,
CDN e helper4grupos passaram. Música ainda não validada nessa imagem.

Espera seguinte600s terminou sem aceite de SPOTIFY_PAIR observado, apenas
rede/Google normais. Não é falha de anúncio: a etapa1 nem iniciou. Helper
reaberto com prazo opcional até3600s; continua enviando a cada2s somente até
aceite. Liberação física60s no painel, janelaLAN5min e demais gates mantidos.
Teste host simula301s antes do primeiro envio dentro do prazo3600s.

Ensaio com etapas de mídia: conta/Connect79,4s; Load com10referências80,4s.
Metadata200/1parte/1.090B em80,8s;4arquivos, máscara de formatos263
(Vorbis96/160/320 e AAC24). Fila avançou0→1→2→3. Em81,3s AP devolveu falha
de chave, payload6B (seq4+erro2); fila6/FAILED, sem storage/CDN nem PCM.
Token HTTP200/199B/551ms só depois, antes152.599B/min127.159B. Logo, a
falha de chave antecede o download e independe do ganho/alto-falante.
Volume no celular gerou frames27, sem áudio. Integridade1, worker38.676B.

Hipótese limitada seguinte: preferir Vorbis160, presente no mesmo metadata,
e registrar somente o código de erro de2B (media stage9), nunca chave/GID.
Há relato primário de falha ao iniciar em96kbps em
[librespot#1433](https://github.com/librespot-org/librespot/issues/1433), mas
também [recusas de chave com outras taxas/contas](https://github.com/librespot-org/librespot/issues/1649).
Nenhum desses relatos prova a causa nesta conta. Não continuar com dados
criptografados sem chave nem repetir indefinidamente pedidos recusados.
Throughput160kbps e capacidade de render/rede ainda precisam ser medidos.

Imagem preferindo160kbps compilada e app-flash verificado,4.107.776B:
bin `2bc7c40cb52074c403974d9406c56c69c125729311d4dfb84c5052a164d31b4b`;
ELF `a55008b98d799b3b498b92fb351494eb9805eabd7a498a1d1563e91ccc69d16c`.
Config/partições/C6/NVS preservados. Cópias privadas regeneradas em outro
diretório coincidem byte a byte com as fontes do diretório de compilação.
Decisão de formato e código da recusa ainda aguardam a execução física.

Primeira captura dessa imagem50s sem tráfego HTTP observado. Nova captura
após reset55s confirmou IP7,8s, provedores habituais200 e Google200/token/
tasks/calendar; min116.587B/after170.863B/maior88.064B/rede4.148B/app2.232B,
integridade1, sem fault registrado. Verificador de placement passou. Isso
valida boot/rede dessa captura, não reprodução ou recuperação genérica.

## Resultado do ensaio160kbps

Pareamento129,0s; conta/Connect130,3s; metadata200/1.090B em131,8s,
quatro arquivos/formats263. Fila0→1→2→3, resposta negativa de chave6B em
131,9s, código16-bit1 (00 01), filaFAILED em132,0s. Segunda faixa selecionada
pelo operador: metadata200/779B em146,1s, mesma máscara e mesma recusa00 01
em146,3s. Token depois200/199B/541ms, antes153.143B/min127.679B/rede4.148B.
Nenhum requeststorage/CDN e nenhum PCM, mesmo com prazo de carregamento
corrigido. Operador confirmou sem som. Amostras de sessão interna154.175B,
maior88.064B, worker38.676B/integridade1. Contadores não exportam conta,
títulos, IDs, chaves, URLs ou token.

Consulta de23/10/2026 encontrou a mesma classe de erro ainda aberta em
[librespot#1649](https://github.com/librespot-org/librespot/issues/1649),
com resumo de mantenedor em9/10/2026 sobre restrições por conta e possíveis
limites de frequência. Isso é contexto externo, não diagnóstico conclusivo
da política aplicada a esta conta. O caso antigo96kbps foi fechado por
[correção de formatos desconhecidos](https://github.com/librespot-org/librespot/pull/1467);
trocar bitrate não resolveu nosso ensaio. Não existe correção validada
na arquitetura atual, e não há motivo para repetir a liberação USB.

Encerrar captura e reiniciar apenasP4 encerra sessãoRAM/anúncio de teste,
mantendo a aplicação normal e preferências. Spotify continua apenas como
flag de laboratório, inativo até provisionamento físico privado; sem
persistência, UIplayer ou alegação de suporte de produto. G3/G6/ADR103
permanecem abertos; throughput e música não validados. Não pedir novas
tentativas sem uma hipótese técnica nova e verificável.

Captura após encerramento55s: IP23,8s depois de reasonsWi-Fi2/205;
recuperação automática, provedores/Google200, mínimo111.015B/após165.331B,
maior81.920B, rede4.152B/app2.236B/integridade1. SD9fotos e preferências
preservados. Sem fault registrado ou início de Spotify nesse boot.

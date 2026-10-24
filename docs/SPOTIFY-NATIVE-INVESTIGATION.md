# Spotify nos alto-falantes do NovaPanel — investigação 22/10/2026

> Histórico encerrado em 24/10/2026 por solicitação do responsável. O produto
> passou a usar [controle remoto na Home](SPOTIFY-REMOTE.md); estes ensaios
> nativos não compõem a imagem atual nem provaram reprodução musical.

O responsável escolheu reprodução no próprio painel e confirmou Spotify
Premium. A alternativa avaliada é cspot, um cliente Spotify Connect não
oficial. A preferência não é por controle remoto de outra caixa.

**Estado em 23/10:** imagem completa com ensaio limitado de pareamento
compilada e gravada no P4, boot e regressão de rede observados. Bibliotecas
Connect são vinculadas somente nessa variante opt-in. Há caminho privado
USB → validação do app/AP → descoberta LAN → autenticação de conta. Após
liberação física, app/AP foram validados na placa; o operador selecionou o
receiver no celular e a conta autenticou (fase 6). Esse primeiro ensaio
mantém reprodução desabilitada. A variante separada `-Playback` acrescenta
decoder Vorbis limitado e saída PCM pelo owner existente; foi compilada e
gravada. Os testes físicos seguintes confirmaram recusa da chave de áudio,
sem reprodução; o resultado atual está abaixo.
O app vazio do harness nunca é gravado. Procedimento e evidência em
[SPOTIFY-PAIRING-VALIDATION.md](SPOTIFY-PAIRING-VALIDATION.md).

**Resultado físico atual — reprodução bloqueada:** decoder Vorbis do candidato passou no P4 com fixture
próprio, conta/Connect autenticaram e o token de mídia retornou200. Metadata
da música retornou200, mas a solicitação de chave do arquivo96kbps recebeu
resposta negativa do AP, antes de storage/CDN/PCM. Não há música audível
confirmada. A tentativa em160kbps também recebeu erro de chave00 01 em duas
faixas distintas (metadata1.090B e779B), sem storage/CDN/PCM. O operador
confirmou ausência de som. Não repetir pareamento ou declarar integração
concluída. A pesquisa encontrou recusa semelhante ainda aberta no
[librespot#1649](https://github.com/librespot-org/librespot/issues/1649), com
discussão recente de9/10/2026; isso não prova qual restrição específica se
aplica a esta conta. Sem correção validada para o cliente/P4 atual.
Procedimento, hashes, códigos e limites em
[SPOTIFY-PLAYBACK-VALIDATION.md](SPOTIFY-PLAYBACK-VALIDATION.md).

## Fontes e versões

- [cspot original](https://github.com/feelfreelinux/cspot): cliente para
  ESP32, exige Premium. O protocolo e autenticação mudam; o README não
  comprova funcionamento atual nesta placa.
- [fork philippe44/cspot](https://github.com/philippe44/cspot), revisão
  `99e5c95ad6d862a1b7845dda81fd9c8233b45b9f`.
- Bell `46a4a8f21f3b0bc0ccefd01915b24d74f0ab9bef`. Pins transitivos de mdnssvc
  e nlohmann/json em `tools/spotify_native_probe/source-lock.json`.
- [aes67-ESP32-P4](https://gitea.apointless.space/bsncubed/aes67-ESP32-P4):
  referência do autor para outro P4/IDF 5.5 com esse fork. Usa Ethernet e
  saída AES67, não o Hosted/Wi-Fi/codec do nosso conjunto. A referência é
  evidência externa, não um teste nosso de reprodução.
- [Waveshare S3 com ES8311](https://github.com/VitaliBorys/cspot-waveshare-esp32s3):
  exemplo de saída de áudio, mas usa outro SoC/pinout e IDF 6.0.1. Não copiar
  os GPIOs nem migrar nosso baseline para acomodá-lo.
- [Spotify comercial](https://developer.spotify.com/documentation/commercial-hardware):
  eSDK oficial depende de parceria; atualmente aceita organizações.
- [Soloist](https://developer.spotify.com/documentation/soloist): alternativa
  oficial para Linux ARM/x86; não é um binário para nosso P4/ESP-IDF.

A licença efetiva de philippe44/cspot é **GPL-3.0-or-later**, conforme
LICENSE.md baixado. Não usar a declaração de MIT de um exemplo diferente
para classificar este candidato. Dependências conservam suas próprias
licenças. Sem redistribuição de firmware nesta investigação.

## Compilação reproduzível

`tools/spotify_native_probe` é um projeto ESP-IDF separado. O runner verifica
os quatro commits. Clone local e venv de geração ficam em diretório ignorado
`firmware/build/spotify-investigation-20261022`. Fontes baixadas não foram
editadas; cópias de compatibilidade ficam só no build.

IDF 5.5.4, target explícito esp32p4, toolchain GCC 14.2.0_20260121,
CMake 4.0.3, Ninja 1.12.1. Python do IDF continua o existente; geração de
protobuf usa Python 3.11 isolado com versões em generator-requirements.txt.
mDNS 1.5.3 vem do conjunto já resolvido do produto.

Adaptações do harness: excluir assembly Xtensa; mapear nome antigo da
prioridade pthread; habilitar exceções somente nos alvos externos;
compilar Vorbis e MP3, excluir fontes AAC/Opus que o GLOB incluía mesmo
desabilitadas; incluir cstring no URLParser; compatibilizar assinatura de
fechamento CivetWeb (const com adaptação ao callback legado). Não silenciar
warnings globalmente. Essas correções não resolvem os gates de runtime.

```powershell
tools/run_spotify_native_probe.ps1
python tools/spotify_client_host_test.py
```

O alvo construído é a biblioteca `cspot`, que também compila Bell e suas
dependências. Não há link/execução do player completo. O app_main vazio é
somente boilerplate de descoberta do ESP-IDF; **nunca gravar esse app**.
Os arquivos .a contêm debug e todos os objetos: seu tamanho não é uso de
flash/RAM do player. Logs locais: configure.log, compile.log,
proto-install.log e artifact-hashes.json. Falhas iniciais de seleção de
codecs/headers/callback foram corrigidas no harness.

## Gates concretos antes de incorporar ao firmware

| Área | Achado no código | Adaptação necessária |
|---|---|---|
| HTTPS único | AccessKeyFetcher/TrackQueue/CDNAudioFile usam HTTPClient de workers próprios, com keep-alive e prefetch. Não passam pelo executor NP2. | Token, metadata e chunks CDN devem ser atendidos pelo executor único; prioridade, timeout, cancelamento e saturação explícitos, sem manter uma conexão CDN aberta ao atender outra. OTA continua exclusiva. |
| TLS | TLSSocket permite VERIFY_NONE se o bundle não foi carregado. | Substituir/adaptar transporte para verificação obrigatória de CA/hostname e qualidade de hora do produto, sem fallback permissivo. Ensaiar certificado inválido. |
| Memória | HTTPClient::readRawBody aloca pelo content-length recebido; estruturas usam vectors/JSON e workers próprios. | Caps de corpo/JSON/filas, admissão por SRAM interna/maior bloco, buffers de áudio com dono. Exceção de PSRAM só após medição/documentação. |
| Segredos | CDNAudioFile registra a URL de CDN; contexto exporta credencial de sessão. | Remover esses logs, redigir erros, vault privado e persistência via FlashCoordinator. Nunca expor URLs assinadas, tokens ou blobs no AppState/USB dump. |
| Áudio | O dono atual usa ES8311 via BSP, PCM16 mono 22.050 Hz; não há streaming de música nem arbitragem de fontes. | Um só dono do codec/I2S, PCM limitado, conversão de canais/taxa conforme decoder, alarmes prioritários, stop/recovery sem bloquear LVGL ou I2C/touch. |
| Controle | cspot tem tasks/callbacks e exceções. | Adaptador privado que captura falhas e publica eventos limitados; somente app_loop escreve estado e somente LVGL toca UI. Nenhum callback de toque faz rede/persistência. |
| Autenticação atual | Este fork requer client_id/client_secret de app próprio para token CDN, além da sessão Connect do usuário. | Validar app local, depois pareamento Premium e acesso real ao CDN. HTTP 200 do token de app não prova login, autorização do CDN nem reprodução. |
| Bancada | Compilação não mediu Hosted+Wi-Fi+decoder+display. | Medir memória mínima/stack, áudio contínuo, pausas/trocas, alarmes, recuperação AP/C6, SD e UI. Preservar G2/G3 e configuração do P4/C6. |

Não há impedimento de arquitetura RISC-V demonstrado pela compilação. A
compatibilidade de reprodução permanece **não comprovada**. Os gates acima
impedem uma gravação de player nesta etapa; não são um pedido de dispensa
de segurança ao usuário. Os defaults/locks/partições do produto continuam
inalterados nesta investigação. ADR-103 delimita a decisão.

## Preparar o app Spotify para o teste autenticado

1. Entre no [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
   com a conta Premium e crie um app pessoal chamado **NovaPanel**.
2. Se o formulário solicitar API/SDK, selecione Web API. Se exigir redirect
   URI, use `http://127.0.0.1:8765/callback`. O teste client credentials abaixo
   não usa redirecionamento nem abre um servidor nesse endereço.
3. Nas configurações do app, obtenha Client ID e Client Secret. Copie o
   exemplo `tools/spotify_native_probe/spotify-client.example.json` para
   `credentials/spotify-client.json`, diretório ignorado, e preencha somente
   no computador. **Não envie o segredo, token ou senha no chat.**
4. Avise que o arquivo local está pronto. Validação no PC:

```powershell
python tools/check_spotify_client.py
```

O helper usa HTTPS com CA/hostname padrão do sistema, endpoint fixo de
token, POST/Basic Auth, timeout de socket de 8 s, corpo máximo 8 KiB e sem
seguir redirects. Imprime apenas resultado/HTTP; não imprime nem salva o
token. Arquivo tem limite de 2 KiB e somente dois campos. Não copia nada
para a placa. Testes offline cobrem limites, falhas de transporte, formato
de token e saída sem segredos; não houve chamada autenticada nesta etapa.

Client credentials é uma credencial do **app**, não autorização do usuário.
O pareamento Connect e a reprodução ainda dependerão do player adaptado e
da autorização pelo app Spotify. Nenhum token de outro aplicativo será
reutilizado para contornar acesso. Procedimento conforme
[Client Credentials](https://developer.spotify.com/documentation/web-api/tutorials/client-credentials-flow)
e [Redirect URIs](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri).


## Resultado do ensaio limpo inicial, anterior à ponte HTTPS

Compilação das bibliotecas concluída em diretório novo `clean-build`,
996 etapas, zero erros. Há **45 warnings externos**, conservados em
`compiler-warnings.txt`; incluem variáveis possivelmente não inicializadas
em Tremor (`bitwise.c` e `vorbisfile.c`), shift de valor negativo e índices
char, além de inicializadores/variáveis não usados. Esses avisos exigem
revisão antes de executar conteúdo de áudio. Não se declarou build livre
de warnings nem decoder qualificado. Reconfigurações anteriores ficam em
`configure-incremental.log`/`compile-incremental.log`.

| Artefato do ensaio, com debug | Bytes | SHA256 |
|---|---:|---|
| libcspot.a | 13.287.532 | `D9B61017807AF6781140B1BABB1AF967C5BF581CAA9150F868FF23F037409422` |
| libbell.a | 24.156.620 | `90018BF89321F21929203BA3E4C4A6CF794B49BBBB77D32E443574E53562930F` |
| sdkconfig do ensaio | 80.370 | `6A6B31FFB7B61014558439884F13F872D847B57C4CA63A80DDDBF426DF4118FF` |
| Manifesto dos 12 arquivos do harness/helper | 1.306 | `44C4C1243E82AC6FFACA81FE75FF5A887901F27EF028216BBA641C3E5C301C3F` |

Configuração efetiva: esp32p4/RISC-V, revisão mínima 100/less-v3, flash
32 MiB, TLS em SRAM interna, auto-suspend desativado. Suporte a stack
externa pertence exclusivamente ao ensaio, para as classes Bell que o
pedem. Não é aprovação desse uso nas tasks do produto.

Teste offline do helper: três grupos passaram (múltiplos casos por grupo):
respostas/limites, transporte/redirect, arquivos/saída sem segredo. Primeira
execução encontrou restrição do sandbox no diretório temporário do Windows;
o fixture passou a usar um subdiretório gerado e verificado dentro de
`firmware/build`, e os testes passaram. Logs em client-host.log. Template
local criado em credentials/spotify-client.json, ignorado pelo Git, sem
sobrescrever arquivo existente; placeholders são rejeitados sem chamada
HTTP. Preenchimento local confirmado pelo responsável; credencial do app validada em 22/10/2026, conforme registro abaixo.

Leitura adicional de CDNAudioFile encontrou que readCapacity vem do header
HTTP e é usado para ler no vetor de 14 KiB sem conferir capacidade. Somar
validação estrita de status/range/tamanho e testes de resposta excessiva ao
gate de transporte. Compilar não demonstra resistência a corpo inválido.

Nenhuma aplicação do produto foi compilada/gravada nesta investigação,
nenhum P4/C6 foi reiniciado e nenhum segredo foi solicitado no chat.
Os gates de transporte, parser, decoder, áudio e bancada continuam pendentes.


## Validação autenticada do app — 22/10/2026

Após o responsável confirmar o preenchimento de credentials/spotify-client.json,
executado tools/check_spotify_client.py no PC. Endpoint oficial de token
respondeu HTTP 200 e o helper validou o formato da resposta, encerrando com
código 0. Saída limitada a status: nenhum Client Secret, Authorization ou
token foi impresso ou salvo pelo helper. Arquivo privado continua ignorado;
não foi enviado ao painel nem incorporado em imagem de firmware.

Isso comprova o par de credenciais do app no fluxo client credentials.
Não comprova login Connect da conta, Premium por API, acesso ao CDN, áudio
no ES8311 ou funcionamento da UI. Os gates de transporte, memória/parser,
decoder, dono de áudio e bancada permanecem abertos. Nenhum player gravado
nesta etapa; a gravação de um player ainda depende dessas adaptações.

## Ponte HTTPS e cliente adaptado — 22/10/2026

ADR-104 acrescenta spotify_http_policy/transport/bridge ao produto. O
executor existente ganhou um modo privado, sem task nova. Entradas são
copiadas para SRAM, fila tem um slot e referências mantêm o pedido vivo
se o chamador expirar antes do worker. DNS/NTP recusados também concluem o
pedido. Nenhum consumidor do player foi iniciado: esta interface ainda
não foi exercitada com requests Spotify na placa. O cliente de ensaio pode
ser ligado à ponte por np_spotify_bind_http; isso não é feito pelo produto.

No ensaio, HTTPClient foi substituído e TLSSocket tornou-se indisponível.
JSON de resolve/token/storage usa o tokenizer limitado já usado na agenda,
com workspace explicitamente interno, campos/tipos/TTL/hosts verificados
e erro sem corpo/URL. Não alterar hooks globais de cJSON. Token tem backoff
de 30 s e formulário privado é limpo após uso. Cópias geradas de TrackQueue
e CDNAudioFile removem logs de URL de CDN e verificam tamanhos antes de
subtrações/cópias. Ler os últimos bytes do arquivo deixa de retornar EOF
prematuro. TrackPlayer confere o resultado de ov_open_callbacks e o formato
inicial 44.100 Hz/estéreo. Fontes fixadas continuam intactas; prepare_compat.py
recusa anchors diferentes. São adaptações de ensaio, não qualificação do
decoder, protocolo Connect ou ciclo de vida das tasks Bell.

Testes host executados com -Wall/-Wextra/-Werror:

- run_spotify_http_host_test.ps1: URLs/headers/ranges, redirects, tamanhos,
  stream sem cópia, parser real do IDF recusando cabeçalho fragmentado acima
  de 8 KiB e codec de resposta (campos ausentes/duplicados, TTL, profundidade).
- run_spotify_bridge_host_test.ps1: timeout com worker ainda ativo ou tardio,
  slot ocupado, cópia de entrada, rejeição da fila, DNS/TLS, alocações,
  piso de memória, chunked, compressão, corpo excessivo/truncado e liberação.
  Transporte/RTOS são simulados; isso não mede um handshake real do Spotify.
- run_spotify_cdn_host_test.ps1: executa a cópia real de CDNAudioFile com
  transporte e cifra identidade. Verifica header/footer, troca de tamanho,
  cache, EOF, seek inválido e falha de download. Não testa AES nem Vorbis.
- Regressões de buffers internos da rede, Google sync, storage/vault e
  mailbox da agenda passaram. Nenhuma credencial foi exibida ou enviada
  à placa nesses testes.

Bibliotecas adaptadas compilaram para esp32p4/IDF 5.5.4. O incremental final
reporta dois avisos já existentes de variáveis não usadas em TrackQueue;
os avisos do decoder no ensaio limpo inicial não foram eliminados. Não
declarar zero warnings externos nem resistência a áudio malformado.

### Firmware e bancada de regressão

Build limpo do produto em firmware/build/spotify-transport-20261022,
2.207 etapas, zero warnings registrados. Incremental final após recusar
espaço em URL, seguido de app-flash COM8/460800, apenas offset 0x20000;
hash verificado. Imagem 3.542.608 B; 58% livres na partição de 8 MiB.
Configuração efetiva preservada (incluindo auto-suspend desativado,
três framebuffers/TRIPLE_PARTIAL). Única mudança de compilação transversal:
HTTP_MAX_HEADER_SIZE=8192 no parser do IDF, confirmada no compile_commands.
Dados estáticos internos 64.024 B, BSS externa 136.336 B; checker passou.

Placa Waveshare P4 v1.3/revisão 103, NOR/PSRAM 32 MiB; base Git
777abde7069c91cfb6aeb915891cf00bc249f9df com alterações locais preservadas.
C6 reportou 3.0.6, imagem não alterada e SHA instalado não obtido. Não
declarar hash C6 comprovado. Manifesto de 291 arquivos públicos em
sources.sha256; credenciais e managed_components não entram no manifesto.

| Artefato final | SHA256 |
|---|---|
| P4 bin | `F7772D75BDE5F922E5CCF7EE6CC839B374ECE510979717813236CF7F393694BD` |
| P4 ELF | `16DDB986DEE9EADC305EDFF4B3AC81483F6453952F06F0D916684CA99ED07AE7` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições bin | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| libcspot.a do ensaio | `0B8DFAFE161CBFC172FC9153E99746684B30DD6EF0216C9990CA85D0983D0EE3` |
| libbell.a do ensaio | `3CAC279E4CAE8CF275356B57E6EE53898D7C817C2882E0A567DF3E7200EC3663` |

Captura final sanitizada de 180 s sem marcador de panic/assert/watchdog.
SD com nove fotos, publicação de imagem e melodias com resultado 0.
Open-Meteo, BCB, Alternative.me e três chamadas brapi retornaram 200;
o domínio BTC concluiu sem chamada CoinGecko nesta captura (não declarar
HTTP CoinGecko ensaiado). Google token/Tasks/Calendar 200, resultado 0,
mínimo interno 120.375 B, depois 174.615 B, maior bloco 102.400 B, rede
livre 4.152 B, app_loop 2.224 B e integridade 1. COM8 liberada ao fim.
Logs e artifact-hashes.json ficam no diretório ignorado do build.

Houve uma tentativa não executada por falha da revisão automática devido
ao limite de uso. Após o horário de liberação informado e novo continue do
responsável, o build/flash final foi executado normalmente. Não houve bypass.

### Trabalho ainda necessário para reproduzir no painel

Integrar a sessão/pareamento Connect e provisionamento privado do app;
limitar servidor local, protocolo e callbacks; resolver exceções/ciclo de
vida de tasks; auditar avisos e operações do decoder; ligar o decoder à
entrada PCM preparada no dono do ES8311 (ADR-105). Medir memória/stack com a biblioteca ativa e testar
throughput de CDN: a ponte atual fecha a conexão a cada request de 14 KiB,
o que não comprova vazão para música contínua. Streaming terá de respeitar
o mesmo executor e concorrer com agenda/OTA sem conexões simultâneas.

Não há login Connect, áudio Spotify, controles de player nem credencial do
app dentro do painel. Não marcar a funcionalidade como concluída, fechar
gates de render/SD/recovery ou declarar release a partir desta regressão.

### PCM e revisão de pareamento, após a regressão de rede

Entrada PCM, conversão e arbitragem preparadas e ensaiadas com sinal
sintético na placa; responsável confirmou audição normal. Autoteste fica
em build explicitamente opt-in e foi removido da imagem normal. Evidência,
limites, hashes finais e pendências em [SPOTIFY-AUDIO-PIPELINE.md](SPOTIFY-AUDIO-PIPELINE.md).
O serviço ainda não é alimentado por cspot nem aparece como player.

Nova revisão do LoginBlob encontrou que checksum divergente apenas gerava
log e o decode continuava, além de slices/varints sem validar tamanho.
prepare_compat.py agora gera cópia que recusa checksum divergente, limita
blob/key, exige bloco ECB completo e extrai auth por helper limitado. A
resposta Zeroconf da configuração cJSON não devolve username em availability
e usa deviceType SPEAKER. Fontes originais continuam intactas. Helper de
política passou corpus determinístico de 10.000 entradas e testes de
tamanhos/checksum/username. Isso testa política sobre dados já decodificados,
não AES, derivação de chave ou pareamento real. Biblioteca adaptada compilou;
libcspot.a mais recente SHA4004DCB75F53A5539BFFF5DEAB040E7C4000B0D87394434F258B489E1AD67719.
Servidor local, protocolo TCP, ciclo de tasks e throughput ainda exigem
adaptação antes de anunciar um receptor Connect no painel.

# Spotify — controle remoto

Direção aprovada em 24/10/2026: encerrar a reprodução nativa experimental e
usar a Web API oficial para controlar Spotify em outro dispositivo. O painel
não é uma saída Spotify Connect, não baixa músicas e não produz PCM Spotify.
As investigações anteriores permanecem como histórico, sem compor este build.

## Uso

Home → deslize o card esquerdo para a esquerda para abrir Spotify; para a
direita, Clima. Dois pontos indicam o widget ativo. A troca é manual e o
último escolhido é persistido. Não há página exclusiva nem troca automática. A tela mostra faixa, primeiro
artista/show, dispositivo, posição, duração e volume. Oferece reproduzir,
pausar, anterior/próxima, seek e seleção de saída em modal. Volume só fica
disponível quando o dispositivo declara suporte. Dispositivos restritos não
recebem comandos. A transferência usa `play:false`: selecionar a saída não
inicia som inesperadamente; depois, toque em reproduzir.

É necessário Premium para os controles. Em modo de desenvolvimento, a conta
precisa estar autorizada em User Management no dashboard do próprio app.
Abra o Spotify no celular/computador/caixa para disponibilizar uma saída.
Nem todo dispositivo aparece na API; 204/404 não são confirmação de reprodução.

## Vinculação pessoal

1. No app existente em <https://developer.spotify.com/dashboard>, registre
   exatamente `http://127.0.0.1:8765/callback` em Redirect URIs.
2. Mantenha o Client ID no arquivo local ignorado
   `credentials/spotify-client.json`. O helper ignora Client Secret; não o
   transfere ao painel. Não envie senha, segredo ou tokens ao chat.
3. Execute, na raiz do projeto, com o Python ESP-IDF que contém pyserial:
   `python tools/link_spotify_remote.py --port COM8`.
4. Autorize no navegador os escopos `user-read-playback-state` e
   `user-modify-playback-state`. O helper usa Authorization Code + PKCE S256,
   callback loopback, state aleatório e teste GET de acesso à API.
5. Quando o helper indicar autorização validada, abra Home → Spotify →
   ícone de engrenagem → Vincular conta. A janela física USB dura 60 s. O helper espera até 20 min
   pelo toque, sem resetar a placa; a autorização fica somente em sua RAM.
6. Aguarde confirmação da gravação. ACK USB sozinho significa enfileiramento,
   não persistência concluída. O widget passa a consultar a reprodução enquanto estiver
   visível na Home. O PC pode ser desligado após vincular.

O vínculo nativo anterior não fornece esses tokens/scopes: esta autorização
é necessária uma vez. Expiração/revogação posterior pede nova vinculação.

## Concorrência, persistência e limites

- UI publica uma intenção de 12 bytes pelo EventBus. `app_loop` é o único
  escritor da projeção separada; não aumenta a cópia já grande da UI geral.
- `spotify_remote.c` usa `esp_http_client` **somente dentro de
  `spotify_remote_process`**, chamado pelo worker existente em
  `NETWORK_VALIDATION_MODE_SPOTIFY_REMOTE`. Token, API e capa são sequenciais
  no mesmo job. Não há task HTTP Spotify, TLS concorrente ou servidor novo.
  Agenda, providers e OTA conservam o executor compartilhado.
- Polling de `/me/player` a cada 5 s apenas com o widget Spotify visível na Home; parado
  no Clima, outras páginas e quadro de fotos. Após comando, consulta em aproximadamente 1 s. Worker ocupado
  rejeita submissão, com retry limitado da leitura; comandos não são
  repetidos automaticamente. 429 respeita Retry-After, até 24 h; falha
  transitória espera 30 s. 401 interrompe polling até nova credencial.
- Access token privado em SRAM, renovado antes de expirar. Refresh token
  novo substitui o anterior quando fornecido; ausência do campo conserva o
  anterior. Rotação mantém uma cópia RAM privada e reenvia a proposta ao
  FlashCoordinator quando ocupado; o polling aguarda essa persistência.
  NVS `np2_spotify`, duas gerações com CRC, commit e verificação. Um corte
  entre rotação no servidor e commit local ainda pode exigir reautorizar.
- Provisionamento bloqueia novas chamadas enquanto a gravação está pendente;
  não substitui credenciais no meio de um job. Nenhum segredo atravessa
  AppState, EventBus, UI, logs ou arquivos gerados pelo helper.
- No perfil de desenvolvimento, retenção NVS segue a exceção já existente.
  Não equivale à proteção física de produção; eFuses não foram alterados.
- HTTP/JSON internos: corpo até 8 KiB, parser com 512 tokens/profundidade 8,
  job de até 18 s, redirects proibidos, CA/hostname/tempo verificados.
  Admissão exige workspace + 64 KiB TLS + piso de 80 KiB; medir mínimo local,
  maior bloco, integridade e pilha. Falta de memória recusa a operação.
- Até seis saídas, IDs de até 128 bytes, títulos/artista de até 192 bytes.
  Cada comando aponta explicitamente para a saída amostrada. Após transferir,
  o alvo antigo é invalidado até a leitura seguinte. Seleção em
  modal carrega revisão para rejeitar índice de lista substituída.

A seleção do widget usa NVS `np2_home` com duas gerações/CRC e gravação
coordenada após 5 s sem alteração, limitada a uma vez por minuto. O registro
é separado dos controles existentes, preservando a leitura em rollback.

## Capa

Prefere a maior variante anunciada até300×300 px. Allowlist exata
`https://i.scdn.co/image/<40 hex>`, sem query/redirect e **sem Bearer header**.
JPEG até32KiB. Se exceder tamanho/memória, falhar ou não decodificar, usa a
variante anunciada até64px quando disponível; 429 respeita Retry-After sem
fallback imediato. Falha de imagem não bloqueia controles. Cache da capa
atual em RAM. Quando usa fallback, tenta melhorar a resolução no máximo uma
vez por minuto com o widget visível, mantendo a imagem atual durante a
tentativa. Após obter a maior, não a baixa novamente a cada poll. Uma resposta
204/sem item não deve deixar um prazo de falha antigo bloqueando a capa ao
retomar o mesmo álbum; Retry-After explícito de429 é preservado.

JSON/corpo normal permanece8KiB. JPEG maior usa buffer SRAM transitório,
limitado ao Content-Length (ou32KiB se não declarado) e uma sentinela, só
após handshake/headers e checagem de reserva de80KiB mais maior bloco. A
extensão é liberada em sucesso, fallback/erro e fim do job. Não usa PSRAM
para HTTP/TLS/JSON e não abre conexões concorrentes. Prazo total18s mantido.

TJpgDec vem do checkout LVGL9.5 já fixado, com cópia privada/configuração
própria. Pool8KiB, sem alocação no decoder, rejeição de dimensões acima300px
antes de decodificar. Dois slots RGB565 até300×300 fixos em PSRAM (384128B com
headers/padding alinhados em64B) usam leases: não sobrescrever a imagem em uso. A UI conserva o
descriptor, invalida cache e libera lease ao trocar/sair. Stride é arredondado
para64B (640B em300px) desde o decode, evitando cópia de ajuste no pool LVGL
(ADR-115).

A pedido do responsável, o layout voltou ao fundo integral aceito: COVER
560×376, antialias, cantos do card e camada preta190/255. Texto em512px e
controles permanecem à frente; sem degradê, viewport lateral ou miniatura.
A melhoria de resolução300px é preservada. Sem capa, superfície padrão.
ADR-114 substitui a parte visual do ADR-113.

## Verificação reproduzível

- `tools/run_spotify_remote_host_test.ps1`: HTTP fragmentado, limites,
  token/cache, parsing, 204/401/429, Retry-After, revisão, memória e vault.
  O shim NVS é preparado por `tools/run_google_agenda_sync_host_test.ps1`.
- `tools/run_spotify_cover_host_test.ps1 -FixturePython <python-com-Pillow>`:
  cores, JPEG inválido/progressivo/truncado, tamanho, leases e 10 mil entradas.
- `tools/run_spotify_remote_ui_host_test.ps1`: LVGL real, 100 ciclos,
  desconectado/offline, janela física, comandos e modal. Render FULL existe
  apenas no fixture desktop; hardware continua TRIPLE_PARTIAL/três buffers.
- `tools/run_app_event_bus_host_test.ps1`: ownership do novo comando e
  regressão do barramento com 8 mil snapshots concorrentes.
- `tools/run_spotify_remote.ps1 -Flash -Port COM8`: build P4 explícito,
  reprodução/pareamento nativos e probe de som OFF, app-flash somente.
- `tools/capture_agenda_diagnostics.py --port COM8 --reset --seconds 90
  --output <arquivo-ignorado>`: boot e diagnóstico por whitelist sem segredos.

Build/testes host não confirmam o controle real da conta. Evidências físicas
e hashes ficam em BRINGUP-EVIDENCE.md. Renovação após expirar o access token,
reboot com token rotacionado, reconexão, soak e render concorrente exigem
campanha específica; não declarar esses gates concluídos por um único teste.

## Fontes oficiais consultadas em 24/10/2026

- [Authorization Code + PKCE](https://developer.spotify.com/documentation/web-api/tutorials/code-pkce-flow).
- [Renovação de tokens](https://developer.spotify.com/documentation/web-api/tutorials/refreshing-tokens): um novo refresh token pode ser retornado; se não vier, manter o anterior.
- [Playback State](https://developer.spotify.com/documentation/web-api/reference/get-information-about-the-users-current-playback).
- [Dispositivos disponíveis](https://developer.spotify.com/documentation/web-api/reference/get-a-users-available-devices).


Validação em placa em 24/10: OAuth/vault confirmados, metadata/capa200,
transferência204 e reproduzir200; operador confirmou capa e controles.
Menor heap interno observado123.743B, worker4.132B livres, integridade1.
A leitura real/capa/controlos iniciais passaram; a campanha longa e renovação
após expirar access token continuam pendentes. Ver BRINGUP-EVIDENCE.md.

Após reboot físico da imagem final, o painel renovou o access token e voltou
a ler faixa/capa com200, sem repetir vinculação. O widget Spotify escolhido
foi restaurado na Home. Isso confirma restauro do vínculo; a resposta não
foi exportada para demonstrar presença de refresh token novo. Rotação
condicional continua coberta pelo teste host, com campanha longa pendente.

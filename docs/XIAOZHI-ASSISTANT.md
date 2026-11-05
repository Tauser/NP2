# Assistente Xiaozhi

Estado em 26/10/2026: a versão inicial conversou por voz com o servidor
oficial na placa P4 v1.3 (COM8, serial terminado 8F42), iniciada por toque.
A revisão ADR-117 com correção de RX passou testes host, build, gravação e
boot. O responsável confirmou "Entrou em Ouvindo e respondeu" na placa;
captura posterior registrou término ESP_OK. Interrupção/fechamento sob carga,
qualidade de áudio e uso prolongado continuam pendentes. Decisão e limites em
[ADR-116/117/118](DECISIONS.md); evidência em
[BRINGUP-EVIDENCE.md](BRINGUP-EVIDENCE.md). Não é gate de release.

O círculo flutuante ADR-118 também passou build, gravação verificada e
testes LVGL. O responsável confirmou "Arrastou e respondeu" na mesma placa
em 26/10/2026, validando arraste e conversa pelo novo acesso. A captura
registrou sessão encerrada ESP_OK, sem perdas de frames indicadas.

Em 26/10/2026 o responsável relatou falhas intermitentes. Na investigação inicial, há registro anterior de timeout após cerca de30s em escuta,
sem perdas contadas e com SRAM acima do piso, mas não foi confirmado que
corresponda ao sintoma atual. Captura passiva150s na imagem ADR-119 não
registrou nova sessão naquela janela.

O responsável esclareceu que houve falhas no início, durante conversa e no
final após silêncio. ADR-120 corrige dois caminhos demonstrados em testes:
áudio recebido conta como progresso do servidor, e o limite de nova escuta
após resposta encerra normalmente. Primeira escuta sem transcrição, conexão,
servidor sem progresso e drenagem têm mensagens distintas. Os episódios
físicos específicos continuam dependendo de reprodução com o novo diagnóstico.

## Uso

- O círculo azul com dois olhos substitui o microfone do header. Um toque
  abre a cápsula e inicia a conversa; o X encerra. A página atual não muda.
- Arraste o círculo para posicioná-lo dentro da tela. O arraste não inicia,
  interrompe ou encerra a voz, nem troca a página. A cápsula acompanha o
  círculo e se adapta ao espaço disponível. A posição vale durante o uso,
  inclusive entre páginas e conversas; reiniciar retorna ao canto inferior
  direito. Não há escrita de preferência em flash durante o arraste.
- Só os olhos animam: piscam, olham e mudam conforme preparação, escuta,
  pensamento, resposta e erro. O círculo mantém tamanho e cor fixos.
- Após o toque aparece "Preparando o assistente...". Fale quando aparecer
  "Ouvindo... pode falar", após confirmação de codec e primeira amostra
  real de captura, já descartados os buffers DMA antigos.
- Tocar no círculo enquanto o assistente responde interrompe a resposta.
  Durante preparação/escuta/pensamento/ativação, outro toque encerra;
  após um erro, o toque tenta novamente. O círculo continua visível quando
  a cápsula fecha; fica oculto no boot.
- Primeira escuta espera até 20s sem atividade; após uma resposta a janela
  de continuação é 5s. Há limites de 30s por escuta e 90s por sessão.
  O nível >=500 é indicador conservador de atividade, não um VAD validado.
- Durante a conversa, consultas de fundo esperam. Comandos interativos
  Spotify e pedidos OTA podem reservar o próximo trabalho e encerrar a voz
  cooperativamente, mantendo um único TLS. O polling Spotify não interrompe.
- Fechar mostra encerramento até confirmação; STOP tem reserva no EventBus
  mesmo com a fila comum cheia. Uma reprodução PCM local ativa é preservada:
  o assistente recusa o conflito, sem pausar/retomar implicitamente.

### Ativação

Na primeira conversa de uma placa nova a cápsula mostra "Ative em
xiaozhi.me" e um código de 6 dígitos. No site, Console > Adicionar
dispositivo, informe o código. A placa consulta a confirmação a cada 5s por
até 5min e a conversa começa sozinha. Cada consulta devolve o executor;
nenhum WebSocket ou buffer de codec fica alocado na espera pelo código.
Idioma, voz e papel do agente são
configurados no console (use português para os textos aparecerem com as
fontes pt-BR do painel).

## Arquitetura

| Parte | Arquivo | Dono / memória |
|---|---|---|
| Protocolo (OTA, hello, listen, abort, parse) | `xiaozhi_protocol.c` | puro, testado no host |
| Sessão (OTA, ativação, WebSocket, estados) | `xiaozhi_service.c` | worker HTTPS único (`np2_netcheck`) |
| Codec Opus + reamostragem | `xiaozhi_service.c` (`xiaozhi_codec`) | task por sessão, pilha PSRAM 40KiB |
| Captura do microfone | `voice_capture.c` | anel PSRAM 4s; leitura só em `device_controls` |
| Limite de leitura RX | `audio_capture_io.c` | wrapper local: até 20ms só na leitura do dono; demais I2S preservados |
| Reamostrador | `voice_resampler.c` | puro, testado no host |
| Cápsula | `ui/screens/np_assistant.c` | task LVGL; só projeção e intenções |

Fluxo: toque → `APP_EVENT_XIAOZHI` → app_loop (`xiaozhi_command/tick`) →
`network_validation_service_request_xiaozhi()` → worker executa
`xiaozhi_service_process()` até o fim da sessão. Estado público volta pela
projeção `xiaozhi_projection_t` (sem token, URL ou identidade).

Identidade: Device-Id é o MAC STA (o mesmo vinculado no xiaozhi.me);
Client-Id é um UUID derivado do MAC por SHA-256, sem gravação em flash.
URL/token do WebSocket ficam só em RAM entre sessões até o reboot.

Voz adquire uma concessão da fila de áudio; gerações antigas não podem parar
ou escrever sobre uma nova reprodução. Alarmes/melodias mantêm prioridade;
captura durante som local encerra só o assistente. Saturação de mensagens,
frames acima do limite e falha de codec encerram explicitamente a sessão.
Task que não termina fica em quarentena com workspace retido até suspensão
confirmada; nenhuma sessão nova reutiliza seus buffers. Piso SRAM 80KiB,
encerramento preventivo abaixo de 88KiB e admissão por maior bloco.

RX: timeout com prefixo PCM16 par conserva os bytes recebidos; timeout sem
bytes permite nova tentativa, sem bloquear controles por mais de20ms em
cada leitura. Após1s sem progresso a captura falha com erro específico de
microfone. Somente o trecho recebido é copiado para o anel.

## Diagnóstico

Linhas `np2_xiaozhi:` e `device_controls: Microphone` entram na captura
sanitizada de `tools/capture_agenda_diagnostics.py`. Nenhuma linha contém
código, token, URL, MAC ou texto falado. Campos úteis: `mic peak` (nível
do microfone, deve passar de ~500 com fala), `frames` (quadros Opus
enviados), `up_drop`/`down_drop` (filas cheias), `internal_min`.

## Testes

```powershell
./tools/run_xiaozhi_host_test.ps1
./tools/run_xiaozhi_service_host_test.ps1
```

Cobre parse OTA/mensagens (UTF-8, `\u` escapes, truncamento seguro, rejeição
de session_id inseguro e de ws:// sem TLS), builders JSON e resposta do
reamostrador (22,05→16kHz: 1kHz 1,0001, 10kHz 0,0146; 24→22,05 e 16→22,05
planos em 1,0000).

Serviço real com fakes: ativação devolvendo executor, cadência/cancelamento,
alocações falhando inclusive com endpoint em cache, codec preso e reaping,
mensagens fragmentadas/saturação, concessão de áudio, geração de captura,
conclusão rápida e handover interativo. Testa também o código real de
admissão do executor e impede replay do último modo em refresh periódico.
Adapter RX cobre timeout, outra task e chamadas fora do escopo.
Também reproduz o timeout convertido em ESP_FAIL pelo esp_codec_dev,
leitura parcial, espera vazia recuperável, prazo sem progresso e a proibição
de mostrar Ouvindo antes da primeira amostra.

Build e gravação P4 com verificação prévia do layout/slot:

```powershell
./tools/run_spotify_remote.ps1 -BuildDirectory D:/Projetos/NP2/firmware/build/xiaozhi-isolation-20261026
python tools/flash_p4_app_checked.py --build firmware/build/xiaozhi-isolation-20261026 --port COM8 --serial-suffix 8F42 --flash
python tools/capture_agenda_diagnostics.py --port COM8 --reset --seconds 110 --output firmware/build/xiaozhi-isolation-20261026/boot-isolation-sanitized.log
```

Usar Python do ESP-IDF 5.5.4. Sem `--flash`, o helper só confere; a escrita
suporta apenas app no slot ativo identificado pela seleção OTA com CRC.
Tabela instalada deve coincidir com a compilada e com os offsets NP2.
Nenhuma operação desse helper escreve bootloader/tabela/NVS/storage/C6.

## Pendências

- Medir qualidade do áudio de resposta e ajustar ganho do microfone (30dB;
  picos observados 700–5800 em fala normal).
- Soak de memória com muitas sessões seguidas e convivência com Spotify.
- MCP (permitir que o assistente controle brilho, volume, dispositivos).
- Qualificar limites de espera/cancelamento, alarmes, nível de atividade,
  continuidade da conversa ao reconectar e qualidade de áudio na placa.
- Conexão permanente e retomada automática de streaming local ficam fora
  desta etapa; preserva-se o slot TLS e o produtor de áudio existente.

Reteste físico ADR-120 em 26/10/2026: o responsável confirmou “agora foi
normal”. O log registrou transcrição/resposta, nova escuta e encerramento
IDLE convertido em ESP_OK, codec parado e executor liberado. Confirma esse
cenário na placa; falhas de rede/servidor e intermitência prolongada não
foram declaradas eliminadas.

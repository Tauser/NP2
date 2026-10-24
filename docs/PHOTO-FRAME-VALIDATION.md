# Quadro de fotos — 22/10/2026

Foto local em tela inteira, com Bitcoin à esquerda e data/hora à direita,
em widgets independentes. O usuário confirmou na placa que a paisagem e os
widgets apareceram e que um toque voltou à página anterior. Também confirmou
que a foto abriu sozinha por inatividade.

## Uso e preparação do cartão

Em Preferências → Quadro de fotos, Ativar quadro habilita a entrada por
inatividade; Ver fotos abre uma prévia manual. Padrão: 5 minutos sem toque e
troca a cada 2 minutos. Inatividade: 30 s, 1/2/5/10/15/30 min ou 1 h.
Tempo de cada foto: 10/15/30 s, 1/2/5/10/15/30 min ou 1 h. A troca inclui
o tempo de leitura/CRC da próxima imagem. Voltar após define a duração total
da sessão: Não sair (padrão), 30 s, 1/2/5/10/15/30 min ou 1 h. Ao vencer,
retorna à página anterior e reinicia a espera de inatividade. O prazo começa
na abertura da foto, inclusive em Ver fotos; trocas de imagem não o reiniciam.
Widgets abre uma modal com visibilidade,
posição superior/inferior e fundo de cada widget. O tamanho escolhido no
relógio também se aplica ao valor do Bitcoin. O fundo pode ser
transparente ou ter contraste de 24%, 36% ou 60%. Normal usa fonte 48 px;
Grande usa 64 px, tanto no relógio quanto no Bitcoin.

A galeria mostra quatro miniaturas por página, até 64 arquivos. Toque na
foto para habilitar/desabilitar; a marca azul indica a seleção. Nada é
apagado do SD. A escolha é persistida por ID do nome `photoNNN.npf`;
substituir o conteúdo do mesmo nome conserva essa escolha. Novos IDs vêm
habilitados por padrão. Com seleção vazia, prévia e entrada ficam inibidas.

Fundo no Mercado foi removido a pedido do usuário em 22/10/2026. A tela
Mercado mantém suas superfícies usuais; fotos aparecem apenas no quadro
por inatividade e na prévia. Bitcoin e hora vêm da projeção real; dados ausentes ou
hora não confiável têm indicação explícita. Valores do fixture não são
constantes no firmware.

Converter JPG/PNG no computador, com Python e Pillow:

```powershell
python tools/prepare_photo_frame.py C:/Fotos/paisagem.jpg F:/np2/photos/photo001.npf
```

`F:` é exemplo: conferir a unidade do cartão antes de copiar. Usar nomes
`photo001.npf` a `photo064.npf` na pasta `np2/photos`. O conversor aplica
orientação EXIF e recorte central proporcional para 1024 × 600, sem barras,
e verifica os bytes gravados. Não sobrescreve um arquivo existente. O
firmware lê NPFP v1: header de 24 B, RGB565 little-endian de 1.228.800 B,
tamanho exato e CRC32. Não lê JPG/PNG diretamente.

Inserir o SD antes do boot. O serviço de fotos não formata nem escreve no
cartão; usa a montagem e o worker de SD existentes. Não foi acrescentada
remontagem automática após remoção. Não há download periódico nesta versão.

## Integração e limites

O worker de weather assets lê no máximo 32 KiB por passo de 100 ms. Catálogo
de até 64 fotos, com busca limitada a 256 entradas e reconsulta a cada 60 s.
Leitura incremental e CRC precedem a publicação. Arquivo truncado, bytes
extras, CRC inválido ou falha de memória conservam a última foto íntegra.
Uma única foto não é relida a cada intervalo.

São alocados sob demanda até dois buffers de pixels em PSRAM: máximo
2.457.600 B, sem fallback para SRAM interna. A UI detém um lease; o worker
nunca altera um slot publicado ou ainda em uso. O objeto de imagem do quadro
é atualizado e o cache antigo é descartado antes da devolução do lease.
Na placa desta campanha, com uma foto, foi necessário apenas um buffer.
Troca entre dois buffers e backpressure foram exercitados no host.

Miniaturas usam dois slots de página imutáveis, até 344.064 B adicionais em
PSRAM; quatro RGB565 de 192 × 112 por slot. Amostragem de até 16 linhas
fonte/32 KiB a cada passo, no worker SD existente. Header/tamanho e leituras
são validados; a miniatura não substitui o CRC integral do slideshow. Não
há arquivos extras de thumbnail no cartão. A UI cancela demandas ao sair
da galeria/abrir a prévia e devolve o lease após destacar as imagens/cache.
Galeria oculta não é redesenhada sob o quadro em tela cheia.

A entrada automática permite Home, Clima, Mercado, Pomodoro e Agenda sem
editor. Configurações, câmera/dispositivos, formulários, teclado, drawer,
transições e avisos visíveis bloqueiam a entrada. Um toque completo fecha a
foto sem acionar controles abaixo. Nova notificação e fim de Pomodoro
interrompem o quadro; serviços continuam executando. Brilho e modo noturno
mantêm o comportamento existente.

Preferências passam pelo serviço de controles e FlashCoordinator, sem I/O
de persistência nos callbacks. Payload atual de 24 B, registro de 48 B, duas
gerações e CRC. Leitor aceita payloads antigos de 2/3/4/6/16 B; preserva
os campos existentes e deixa o retorno automático desativado, sem escrita
na restauração. Os sete bytes reservados são explicitamente zero.
Firmware anterior não interpreta 24 B; rollback depende de uma geração
antiga íntegra. Decisões em ADR-100/101/102. Os builds abaixo registram as
versões sucessivas; os primeiros ainda usavam payload de 6 B.

## Testes reproduzíveis

Executados com GCC UCRT64 e LVGL 9.5.0 do componente fixado:

- `tools/run_photo_frame_host_test.ps1`: CRC/header, prefs legadas,
  ausência de cartão/pasta, foto única sem releitura, dois leases e
  backpressure, imutabilidade do publicado, truncamento, bytes extras,
  CRC inválido, falha de alocação e remoção durante leitura passaram.
  O teste de storage usa rotinas reais com backend NVS simulado: migração
  2/3/4/6 B, roundtrip 16 B, commit falho e fallback por CRC passaram.
  Seleção bit 63/vazia, desmarcar durante leitura, páginas/leases de
  miniatura, falha de alocação/remoção e edição durante write em voo passaram.
- `tools/run_photo_ui_host_test.ps1`: fonte RGB565 inteira, 64 combinações
  de widget, dado/hora ausentes, Mercado sem wallpaper, paginação e seleção
  da galeria, limites dos seletores, modal e 100 ciclos LVGL sem perda passaram.
  Renders locais em `firmware/build/photo-ui-host-20261022/` são fixtures,
  não fotografias da placa.
- Regressões `run_sound_melody_host_test.ps1`,
  `run_notification_preferences_host_test.ps1`,
  `run_app_event_bus_host_test.ps1` (8.000 snapshots) e
  `run_google_agenda_sync_host_test.ps1` passaram.
- `tools/check_agenda_memory_layout.py` passou: pools, projeção e snapshots
  continuam nas regiões previstas; credenciais permanecem internas.

## Build e identificação

Base Git `777abde7069c91cfb6aeb915891cf00bc249f9df`, working tree modificado
com as entregas anteriores preservadas. Build novo em
`firmware/build/photo-frame-20261022`, ESP-IDF 5.5.4, target explícito
`esp32p4`. Build final sem warnings. Aplicação 3.531.216 B (`0x35e1d0`),
58% livres no slot de 8 MiB. Dados internos estáticos 63.864 B (+64 B sobre
melodias), BSS externa 133.648 B (+328 B).

| Artefato | SHA256 |
|---|---|
| P4 bin | `C0799670EA10040F3D94693214CC628B7AE807C4955254787B50155067EC59B1` |
| ELF | `51437ED1DA231975B48A677FD22E77B7B2FFE1F5C2600012E206511F6105A9CC` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições geradas | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto de 81 fontes públicos, `sources.sha256` | `FDD6F5BB5BA0B413DBE75CF6DCEEF91F194F3C71F4A9CD662BB44951FB6F541E` |

Com ambiente IDF 5.5.4 exportado, a partir de `firmware/`:

```powershell
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/photo-frame-20261022/sdkconfig -B build/photo-frame-20261022 build
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/photo-frame-20261022/sdkconfig -B build/photo-frame-20261022 -p COM8 -b 460800 app-flash
```

Mesma Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3/revision 103, NOR e PSRAM
de 32 MiB. BSP 3.0.1, LVGL 9.5.0, adapter 0.6.4, RGB565, rotação 180°,
três framebuffers e TRIPLE_PARTIAL preservados. ESP-Hosted 3.0.6,
esp_wifi_remote 1.6.4, RPC v2/SDIO SW_AGGR; configuração e partições
idênticas por hash. Não houve mudança de Kconfig, dependências ou eFuses
para fotos. C6 reportou 3.0.6, não foi regravado; hash instalado desconhecido.

App-flash COM8/460800 somente na aplicação em `0x20000`: 3.531.216 B,
hash verificado e reset. Nenhuma escrita de bootloader, partições ou NVS
pelo comando de flash. Logs/artefatos locais ignorados no diretório do build:
`build-final.log`, `app-flash.log`, `artifact-hashes.json`, `sources.sha256`,
`boot-sanitized.log`, `idle-sanitized.log` e logs dos testes host.

## Bancada e observações

SD FAT32 de 500.137.984 B. A paisagem do mock foi preparada no computador
e copiada para `F:/np2/photos/photo001.npf`, 1.228.824 B; origem e destino
conferidos por SHA256
`A27260D74120FD6004387092799DCD68F5642F9FB24990EE505DDE77B5AF9814`.
Após reinserção e boot, catálogo com uma foto; publicação em 4,7 s de captura,
1.228.800 B de pixels, heap interno livre 183.583 B e PSRAM livre
14.079.980 B naquele ponto.

Google real com foto carregada: token/Tasks/Calendar HTTP 200, conclusão
resultado 0 em 37,7 s da captura, mínimo interno 120.475 B, após 174.771 B,
maior bloco 90.112 B, pilha de rede livre 4.204 B, integridade 1. Agenda
restaurou quatro itens, conta vinculada e nenhuma pendência. Startup e
notificação concluíram com resultado 0. App_loop mediu mínimo livre
2.176 B durante a interação; PSRAM em regime 14.040.552 B.

Prévia manual: transições visible=1/0 registradas; usuário confirmou foto e
widgets visíveis e retorno à página anterior pelo toque. Na segunda captura,
saída em 196,7 s e entrada em 256,9 s (60,2 s depois), seguida da confirmação
do usuário de que a foto abriu sozinha. Esse intervalo é compatível com a
opção de 1 minuto; a configuração efetiva não foi exportada. O padrão do
código é 5 minutos, mas esta captura não comprova cronometragem desse valor.

Captura final até 288,9 s: quatro itens, sem pendência, PSRAM livre
14.040.552 B, interno livre 176.871 B, maior bloco 90.112 B. Outra consulta
Google terminou com resultado 0 e integridade 1. Nenhum marcador de
panic/assert/watchdog foi capturado. Coleta encerrada e COM8 liberada.

Esta campanha não qualifica remoção a quente/remontagem, várias fotos
físicas, SD concorrente sob carga máxima, power cuts, latência/tearing,
áudio instrumental, notificações durante idle ou soak. Esses gates
permanecem abertos; build, fixture e confirmação visual não os encerram.

## Ajuste de tipografia — 22/10/2026

A pedido do usuário, o valor do Bitcoin acompanha a fonte do relógio:
32 px em Normal e 48 px em Grande. A projeção visual normaliza apenas o
bit de tamanho do Bitcoin, inclusive para perfis já salvos; posição,
visibilidade e contraste permanecem independentes. Não exige migração nem
reescrita das preferências existentes. A modal mostra Igual ao relógio no
campo do Bitcoin e conserva o seletor Normal/Grande na coluna Data e hora.
Salvar grava ambos com o mesmo tamanho pelo fluxo existente.

Fixture LVGL passou novamente, incluindo as 64 combinações, fontes iguais
mesmo com flags antigas divergentes, preço de seis dígitos sem quebra,
limite de largura, modal e 100 ciclos. Render inspecionado em
`firmware/build/photo-ui-host-20261022/frame-btc-size.png`.

Build em diretório novo `firmware/build/photo-btc-20261022`, mesma base Git,
placa e ESP-IDF 5.5.4/esp32p4 acima. Um aviso inicial comparava caminhos
Python com barras diferentes no Windows; normalizado o ambiente, o build
final terminou sem warnings. App 3.531.232 B (`0x35e1e0`), +16 B, 58%
livres. SRAM estática 63.864 B e BSS externa 133.648 B, ambas sem aumento.
Verificação de regiões de memória passou; configuração e partições
idênticas por bytes à versão anterior.

| Artefato do ajuste | SHA256 |
|---|---|
| P4 bin | `616254251DCFD640AD176442E04F48A3ED3DA32A3A7C52657A404D364488ED62` |
| ELF | `F4E307CD1B6E2D35BCC648F70CB9330440DAD51BDAF1D5CF5F4B1EAF38211418` |
| Manifesto dos 81 fontes | `91AFE48A91A934510467669C4FC6B92104E27CE3BD3B6D103DD0EA8B1FCEB1B7` |

Comandos de build e app-flash são os mesmos desta página, substituindo
`photo-frame-20261022` por `photo-btc-20261022`. Arquivos locais de evidência:
`build.log`, `build-final.log`, `app-flash.log`, `artifact-hashes.json`,
`sources.sha256` e `boot-sanitized.log` no novo diretório.

App-flash COM8/460800: aplicação em `0x20000`, 3.531.232 B, hash verificado,
reset confirmado; sem gravação de C6/NVS/partições. Boot publicou a foto em
6,2 s da captura, interno livre 183.575 B e PSRAM livre 14.079.980 B.
Startup/notificação resultado 0. Google token/Tasks/Calendar HTTP 200,
sync resultado 0 em 38,6 s, mínimo interno 120.599 B, após 174.823 B,
maior bloco 102.400 B, rede livre 4.200 B, integridade 1. Agenda: quatro
itens, sem pendência; app_loop livre 2.256 B. C6 reportou 3.0.6, hash
instalado desconhecido. A tipografia foi conferida no fixture real LVGL;
esta rodada não adiciona medição física de tearing ou latência.

Entrada do quadro registrada em 77,4 s da captura após boot. Coleta de
105 s encerrada, sem marcador de panic/assert/watchdog; COM8 liberada.

## Remoção de Fundo no Mercado — 22/10/2026

Removidos switch, callbacks, imagem, escurecimento e alteração de opacidade
dos cards do Mercado. O worker recebe demanda de troca apenas do quadro
ou da prévia. Padrão passa de `0x17` para `0x15`; bit 1 legado continua
aceito no perfil, mas não ativa leitura de fotos nem wallpaper. Nenhuma
escrita de migração é disparada no restore, e a próxima edição normal do
perfil deixa de incluir esse bit. Idle, intervalo e widgets são preservados.
O valor do Bitcoin continua com o tamanho do relógio.

Testes host de fotos/storage e UI passaram. O teste de serviço cobre o
perfil antigo com somente o bit 1: nenhuma alocação de imagem, mas prévia
manual continua funcionando. Fixture do Mercado verifica cards opacos e
ausência de imagem de wallpaper; modal, enquadramento e 100 ciclos passaram.

Build limpo em `firmware/build/photo-idle-only-20261022`, IDF 5.5.4,
`esp32p4`, mesma base Git e working tree acima, sem warnings. App
3.530.704 B (`0x35dfd0`), 58% livres. SRAM estática 63.864 B, sem aumento;
BSS externa 133.616 B (-32 B). `s_ui` 14.168 B; verificação de regiões
passou. Configuração efetiva e partições idênticas por bytes à versão BTC.
Comandos desta página substituindo o diretório por `photo-idle-only-20261022`.

| Artefato sem wallpaper | SHA256 |
|---|---|
| P4 bin | `A20AB13AF4EA4E5899CC150447BC5058E45F641D34DC659F317156A82132E2B0` |
| ELF | `00143FFA8FC42FE9BC9AE9577524B1E99E022A42D23B8472B4C4E37F9261E89F` |
| Manifesto dos 81 fontes | `58DC9837A994D02A3D4BCF12F6A4FF5CFFE83105907B202879DF8AD1B005A222` |

Primeira tentativa de `app-flash` em COM8 falhou antes da conexão: porta não
encontrada (`app-flash.log`). Após reencaixe físico confirmado pelo usuário,
COM8 reapareceu e o retry gravou 3.530.704 B na aplicação em `0x20000`,
18,5 s, hash verificado e reset confirmado (`app-flash-retry.log`). Nenhuma
gravação de C6, NVS ou partições pelo comando. Mesma placa P4 v1.3/revision
103, NOR/PSRAM 32 MiB das rodadas anteriores.

Captura sanitizada de 105 s encerrada e COM8 liberada. Foto índice 1
publicada em 6,9 s, heap interno livre 183.595 B e PSRAM livre 14.080.636 B;
catálogo seguinte mostrou nove arquivos. Startup/notificação resultado 0.
Google token/Tasks/Calendar HTTP 200, resultado 0 em 38,1 s, mínimo interno
120.555 B, após 174.811 B, maior bloco 90.112 B, pilha de rede livre
4.200 B, integridade 1. Quatro itens de Agenda, sem pendência; app_loop
livre 2.256 B. C6 reportou 3.0.6, hash instalado desconhecido. Nenhum
marcador de panic/assert/watchdog. Não houve nova confirmação física do
layout Mercado nesta rodada; aparência conferida no fixture LVGL, e os
gates de render/SD/soak continuam abertos.

## Tela própria, galeria e tempos ampliados — 22/10/2026

Implementação da ADR-101: entrada própria após Tela e som em Preferências,
galeria de quatro miniaturas, paginação até 64 IDs e seleção persistente.
O relógio e Bitcoin passaram a 48/64 px. Tema, componentes e modal de
widgets existentes foram mantidos. Tela e som voltou a brilho/volume/modo
noturno. Nada foi acrescentado ao Mercado.

Host de fotos cobre máscara vazia/bit 63, troca de seleção durante leitura,
dois leases de página, backpressure e falha de memória/remoção. NVS real com
backend simulado cobre migração 2/3/4/6 B sem escrita, payload 16 B, falha
de commit e fallback CRC. Teste do owner cobre bounds, novas escolhas,
edição enquanto um write está em voo, retry ocupado e ACK com erro.
Fixture LVGL cobre galeria, último ID 64, seleção vazia, novos seletores,
fontes iguais/maiores, modal e 100 ciclos sem perda. Alocação host de cena:
37.704 B; com modal 55.440 B, com ponteiros de host. Navegação reserva
40 KiB antes de construir a tela na placa e pode descartar caches ocultos.
Isso não substitui medição física de heap/render. Regressões de Agenda,
barramento/8.000 snapshots, som e notificações passaram.

Usuário confirmou na placa: miniaturas, seleção, relógio maior e retorno.
A primeira captura registrou miniaturas da página 0 de um catálogo com
nove arquivos e seleção persistida com resultado 0. O teste alterou
inatividade para 30 min e troca para 1 min; a última máscara salva permitiu
63 IDs (o default permite os 64 IDs possíveis, incluindo ainda ausentes).
Slideshow publicou índices 2/3/4, usando os dois buffers completos.
PSRAM livre em regime com fotos/miniaturas: 12.630.888 B. Google real
token/Tasks/Calendar HTTP 200, resultado 0, mínimo interno 119.647 B, maior
bloco 98.304 B, rede livre 4.168 B e integridade 1. App_loop livre 2.252 B;
quatro itens de Agenda, sem pendência. C6 3.0.6, não gravado/hash instalado
desconhecido. A primeira captura de 251,3 s não teve marcador de fault.

Após esse teste, um ajuste suspende a atualização da galeria sob a prévia
em tela cheia. Não foi feita nova medição comparativa de CPU desse ajuste.
Build final em `firmware/build/photo-gallery-20261022`, base Git
`777abde7069c91cfb6aeb915891cf00bc249f9df` com working tree modificado,
IDF 5.5.4/esp32p4. Diretório novo com build completo e retomada para ajustes
locais; final sem warnings. App 3.539.072 B (`0x360080`), 58% livres.
SRAM estática 63.936 B (+72 B); BSS externa 136.328 B (+2.712 B) sobre a
versão sem wallpaper. `s_ui` 14.440 B, projeção 4.968 B; regiões de memória
conferidas. Configuração e partições idênticas por bytes às anteriores.

| Artefato final da galeria | SHA256 |
|---|---|
| P4 bin | `BD175E6CFAED4088BF388A42CEBB3DBA8FB6C58715D1C23A4BC6BDD3AD27D13E` |
| ELF | `B8232036142973E0650A5AD447ABB57662552F0D631622A8A5DFCC868E110753` |
| Manifesto dos 88 fontes | `9832BB6758E2A66D4DC2AE0734C553F46FAFE5B44A54BBD0C1C81FD54D63CE1F` |

Os comandos de build/app-flash são os mesmos desta página, substituindo o
diretório por `photo-gallery-20261022`. Mesma Waveshare P4 v1.3/revision
103, NOR/PSRAM 32 MiB; BSP/LVGL/adapter/Hosted, três FB, RGB565 e rotação
180° conservados. Primeiro app-flash COM8/460800 somente em `0x20000`,
3.539.072 B, 18,4 s e hash verificado. A primeira imagem tinha bin SHA256
`0E6BDF3BDCBC610D2EDEDC124C0F21C63806906D58BE0EBB7932B1223EC2049C`;
metadados anteriores preservados em `artifact-hashes-first-flash.json` e
`first-flash-sources.sha256`. Logs finais e artefatos locais ficam no mesmo
diretório ignorado. Os gates físicos anteriores continuam abertos.

App-flash final COM8/460800 gravou a aplicação do quadro acima, com hash
verificado e reset (`app-flash-final.log`). Captura imediata após flash:
em 3,4 s o perfil de 16 B foi restaurado com idle 1.800.000 ms,
troca 60.000 ms e 63 IDs permitidos, exatamente a última edição salva pelo
usuário. Brilho 26% e volume 100% preservados. Isso acrescenta evidência de
restore após reboot ao roundtrip/CRC host; não foi feito ensaio de power cut.
LVGL registrou pool total 194.596 B, adição PSRAM 131.072 B, sem mudança de
configuração nessa entrega. Foto índice 1 publicada em 17,8 s; nove arquivos
no catálogo. Google final HTTP 200/resultado 0 em 62,3 s, mínimo interno
120.347 B, maior bloco 79.872 B, rede livre 4.228 B, integridade 1.
App_loop livre 2.252 B, quatro itens de Agenda e nenhuma pendência.
Captura final de 120 s encerrada sem marcador de panic/assert/watchdog;
COM8 liberada. Última amostra periódica em 94,9 s manteve estado saudável.


## Retorno automático — 22/10/2026

ADR-102: seletor Voltar após, sem contagem sobre a fotografia. Não sair é
o padrão; opções 30 s, 1/2/5/10/15/30 min e 1 h. Tempo de sessão independente
da troca de imagem. Retorno usa o mesmo fechamento do toque, preserva a
página anterior e reinicia a inatividade antes de verificar nova entrada.

Host: timer desativado, todas as durações antes/no/depois do limite, wrap
uint32 e nova sessão passaram. Migração 2/3/4/6/16 B sem escrita, seleção
bit63, tempos anteriores, roundtrip 24 B, reservados/índice inválidos,
commit falho/CRC fallback e edição exclusiva de timer durante save passaram.
LVGL: terceiro seletor, layout renderizado, modal e 100 ciclos sem perda;
cena 39.416 B, com modal 57.176 B (ponteiros de host). Eventos/8.000 snapshots,
notificações e regressões Google/Agenda passaram. Comandos:

```powershell
tools/run_photo_frame_host_test.ps1
tools/run_photo_ui_host_test.ps1
tools/run_app_event_bus_host_test.ps1
tools/run_notification_preferences_host_test.ps1
tools/run_google_agenda_sync_host_test.ps1
python tools/check_agenda_memory_layout.py firmware/build/photo-exit-timer-20261022/np2_p4.elf
```

Build limpo IDF 5.5.4, target explícito esp32p4, em
`firmware/build/photo-exit-timer-20261022`, sem warnings de compilação.
Mesma placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B, P4 v1.3/revision 103,
NOR/PSRAM 32 MiB; base Git `777abde7069c91cfb6aeb915891cf00bc249f9df`,
working tree modificado. Manifesto de 88 fontes identifica o conteúdo.
App 3.540.176 B (`0x3604d0`), +1.104 B sobre a galeria; 58% livres no slot.
SRAM estática 63.952 B (+16), BSS externa 136.336 B (+8), regiões aprovadas;
s_ui 14.440 B e projeção 4.968 B. Sem mudança de SDK config, partições,
fontes, pool LVGL, dependências, display ou C6.

| Artefato do timer | SHA256 |
|---|---|
| P4 bin | `C3575FD8CF4EECD0617AB8C60E8A5351A8D7F25FD0248BECA24A8459B3592B67` |
| ELF | `B80B5B77ABB0BC9CF0609102D9EB6D1FA83CFA1BBA566AAAF98D0B7BADF3DF67` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto dos 88 fontes | `3CFEF832E38244D58ADD07AAF7BB742196F58157C77D5317C035E58A6A800300` |

Build/app-flash seguem os comandos anteriores com diretório
`photo-exit-timer-20261022`. Metadados e logs locais nesse diretório
ignorado: artifact-hashes.json, sources.sha256, build.log, app-flash.log,
boot-sanitized.log, service-host.log, ui-host.log e agenda-host.log.
Render LVGL em `photo-ui-host-20261022/settings-exit-timer.png`.
Teste físico confirmado pelo usuário: Voltar após 30 s → Ver fotos retornou
sozinho à tela anterior. Captura: visible=1 em 136,5 s; outra foto publicada
em 142,5 s; timeout e visible=0 em 166,6 s (30,1 s). A troca não reiniciou
o prazo. Sem reabertura imediata no restante da janela. Esta rodada valida
a prévia; repetição física pela entrada automática e durações longas continua
fora desta campanha.
Não fecha gates de render/SD concorrente, power cuts ou soak.


Gravação concluída pela COM8/460800, somente aplicação em `0x20000`:
3.540.176 B em 18,3 s, hash verificado e reset. Não houve escrita em NVS,
partições, C6 ou eFuses pela ferramenta de flash. Em 3,3 s da captura,
o perfil instalado restaurou idle 30 min/troca 1 min, seleção de 63 IDs e
retorno desativado; brilho 26% e volume 100% preservados. Nove arquivos no
catálogo; foto publicada em 13,3 s. Hash do bin novamente conferido após flash.


Durante o ensaio, o painel salvou retorno 30.000 ms (61,1 s) e depois idle
60.000 ms (64,5 s), ambos resultado 0, mantendo troca 60.000 ms e os 63 IDs
permitidos. Essas escolhas do usuário foram mantidas. Google terminou com
HTTP 200/resultado 0 em 45,2 s: mínimo interno 120.455 B, maior bloco
102.400 B, rede livre 4.156 B e integridade 1. App_loop livre 2.224 B,
quatro itens de Agenda e nenhuma pendência. C6 reportou 3.0.6; hash instalado
desconhecido. PSRAM em regime 12.627.680 B com galeria e dois buffers de foto.

Captura sanitizada de 180 s concluída sem marcador de panic/assert/watchdog;
COM8 liberada ao fim da coleta.

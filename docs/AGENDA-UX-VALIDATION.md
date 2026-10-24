# Agenda — revisão de UX de 20/10/2026

## Direção atual — revisão compacta de 21/10/2026

A revisão conserva o shell, a paleta Dark Graphite, fontes bitmap Montserrat,
ícones Material e controles do formulário compartilhado. Não acrescenta
dependências, assets raster ou fontes dinâmicas. A pedido do responsável, a
lista ocupa toda a área de conteúdo; criação e edição usam `np_modal`, como
as outras telas do sistema. A evidência da revisão inicial em duas colunas
fica preservada abaixo apenas como histórico de bancada.

- Seis linhas de 56 px por página, com horário/data em coluna própria,
  título em 20 px e informação secundária em 16 px. Títulos longos usam
  reticências. Seleção tem fundo e contorno explícitos; concluídas e conflitos
  também têm texto, sem depender somente da cor. Ícones e etiquetas distinguem
  Evento/Tarefa; filtros exibem as quantidades por tipo. A segunda linha mostra
  término, pendência/conclusão, lembrete ou conflito conforme o item.
  Cabeçalho compacto reúne criação, filtros, data e controles por ícone.
  Sincronizar fica sempre no topo; conectar só aparece sem conta ou quando
  uma resposta do Google indica necessidade de nova autorização. Não há
  faixa de controles no rodapé. Os 12 itens cabem em duas páginas.
- Filtros Todos/Eventos/Tarefas, contagem filtrada e estado vazio contextual.
  Ordenação por data, sem prazo depois dos itens datados e tarefas concluídas
  ao final. O índice limitado de apresentação não altera a ordem persistida,
  a identidade ou a revisão dos itens.
- O modal diferencia novo/editar, agrupa data e hora de início e término,
  e omite término em tarefas. Placeholder e ajuda acompanham o tipo.
  Salvar é a ação primária; excluir usa o estilo destrutivo compartilhado e
  conserva confirmação em dois toques. Controles de ação têm alvo mínimo
  de 48 px. Conflitos conservam as escolhas Usar Google/Manter painel.
  Cancelar/fechar descarta o rascunho sem enviar comando. Fechar durante um
  comando em voo conserva seu ACK; o estado da lista continua acompanhando
  a persistência. Sucesso de comando fecha o modal, sem afirmar que o dado já
  foi gravado na flash. Falha mantém o formulário e informa também a lista.
- Status global, contagem e feedback da edição ficam em áreas separadas.
  Pendência só termina após confirmação de persistência. Vinculação USB e
  sincronização têm feedback temporário na faixa de status do cabeçalho.
  Feedback comum expira em 8 s; instrução USB, em 60 s, sem timer adicional.
- Todos os cinco campos de texto ficam acima do teclado compartilhado, sem
  deslocamento do painel nem segundo teclado. Fechar libera a sessão de input.
  O modal só é construído no primeiro uso e é reutilizado até sair da tela;
  pertence à raiz da Agenda e seus bindings são removidos na destruição.
  Tarefas usam modal de 400 px, com prazo/hora e lembrete na mesma linha;
  eventos mantêm 480 px. Os botões primário/secundário também são reutilizados
  para resolver conflitos, sem dois botões ocultos extras.

Tocar no status Google abre os detalhes da conexão no mesmo modal, com
último sincronismo e ação física Liberar USB. Assim o helper mantém a
confirmação separada de commit mesmo com o ícone de conectar oculto após
vincular. A janela continua durando 60 s e sendo consumida por um comando;
nenhuma leitura USB passou a ser aceita sem o gesto físico.

Domínio, storage, OAuth, lembretes e políticas de sincronização continuam
usando os contratos existentes. Os filtros são locais e não geram HTTP.

## Validação host e revisão visual da primeira revisão (histórico)

Comando: `./tools/run_agenda_ui_host_test.ps1`.

Passaram criação/edição, validação, conclusão, exclusão confirmada, conflitos,
estados de sync, feedback de storage com falha/retry e confirmação ausente,
filtros/paginação, ordenação sem mutar o domínio, ID do comando após ordenar,
rascunho preservado ao filtrar, seleção e coordenadas do campo acima do teclado.
Os 100 ciclos de criação/foco/destruição terminaram com o mesmo heap livre:
2.070.872 B antes/depois. A construção da tela consumiu 40.728 B no host de
64 bits, dentro do teto existente de 40 KiB, sem ampliar o pool do produto.
Esse número de host não é uma medição de SRAM do P4.

Foram inspecionadas imagens 1024×600 produzidas pelo LVGL real do teste,
com as fontes do produto: evento selecionado, segunda página, nova tarefa
e filtro vazio. Saída RGB565 e PNGs locais ficam ignorados em
`firmware/build/agenda-ui-host-20261019/`, nomes `agenda-ux.png`,
`agenda-page2-ux.png`, `agenda-task-ux.png` e `agenda-empty-ux.png`.
São fixtures de host, sem dados reais da conta; não são fotografias da placa.

As imagens estáticas não qualificam tearing, latência tátil, ergonomia física
ou soak. Os gates e a falha preexistente de filesystem cheio documentados em
[AGENDA-MEMORY-VALIDATION.md](AGENDA-MEMORY-VALIDATION.md) permanecem abertos.

## Build limpo e bancada da primeira revisão (histórico)

Placa Waveshare P4 v1.3 (silício revision 103), flash e PSRAM 32 MiB; base
`777abde7069c91cfb6aeb915891cf00bc249f9df` com alterações no working tree.
Inspeção completa de PCB/BOM/lote não foi repetida nesta revisão.

Build do zero em diretório novo `firmware/build/agenda-ux-20261020`, IDF 5.5.4
e target `esp32p4`, a partir do sdkconfig efetivo da campanha anterior.
Sem warnings. Antes de gravar, conferidos hashes idênticos de configuração e
partições e executado `tools/check_agenda_memory_layout.py`. SRAM estática
continua em 63.752 B; BSS externa passa a 125.688 B (+48 B de estado visual).
Pool LVGL útil no boot permanece em 194.596 B.

`app-flash` COM8/460800 gravou somente o app P4, 3.509.408 B em `0x20000`,
com hash de dados verificado e 58% da partição de 8 MiB livre. Boot restaurou
um item e a conta Google, `ready=1 pending=0 linked=1 sync_busy=0 sync_result=0`.
C6 não foi regravado; o hash da imagem instalada continua não obtido.

| Artefato local | SHA256 |
|---|---|
| P4 bin | `B18A3A14E07CD1C96AC0AF3D785273AC4DF8F787CD3A0683D1A622FE4E91EA43` |
| ELF | `F3B143A229EF5EE4B74D3DDD1A39A5AA09DC95942EAD8E5E6E8E6F4602CAD00E` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Tabela de partições | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto `sources.sha256`, 48 fontes públicos | `5FBC643515FE6A2423ED1561E4683C650859A94EEDC356ABBD37EE6C289D752B` |

Reprodução em `firmware/`, com o ambiente IDF 5.5.4 exportado:

```powershell
idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/agenda-ux-20261020/sdkconfig -B build/agenda-ux-20261020 build
idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/agenda-ux-20261020/sdkconfig -B build/agenda-ux-20261020 -p COM8 -b 460800 app-flash
python ../tools/capture_agenda_diagnostics.py --port COM8 --seconds 100 --output build/agenda-ux-20261020/boot-sanitized.log
```

`build.log`, `app-flash.log`, captura sanitizada e manifesto ficam locais e
ignorados no Git, junto aos artefatos privados de desenvolvimento. Nenhuma
credencial, corpo HTTP ou dado da agenda real foi exportado para os renders.

Na sincronização automática desta imagem, C6 reportou 3.0.6 e token, Calendar
e Tasks retornaram HTTP 200, resultado final 0. Mínimo interno 124.163 B,
maior bloco 98.304 B, pilha de rede mínima 4.212 B e `integrity=1`.
O texto do helper PC acompanha o botão Conectar Google/Reautorizar; seus cinco
testes offline passaram. Essa atualização de texto Python não altera o binário.
A captura de 100 s terminou normalmente e liberou COM8. Em 95,3 s havia um
item, `pending=0 linked=1 sync_busy=0 sync_result=0`, sem fault capturado.

## Revisão de 20/10 — lista em tela inteira e formulário modal (histórico)

Comando host: `./tools/run_agenda_ui_host_test.ps1`. Passaram lista sem
formulário alocado, abertura lazy, cancelar sem comando, fechar durante envio
sem perder o ACK, fechamento automático no sucesso, reabertura sem rascunho
cancelado, X liberando teclado e destruição da tela com teclado ainda aberto.
CRUD, filtros, ordenação/IDs, conflitos, validação, feedback de storage e
100 lifecycles continuam passando. As coordenadas dos cinco campos foram
conferidas acima do teclado. Baseline/final: 2.070.864 B livres idênticos.

Construção da lista no host: **28.680 B**, abaixo do teto existente de 40 KiB
e 12.048 B menor que a versão em duas colunas. Primeiro uso do modal acrescenta
20.304 B; agregado construído de **48.984 B**, testado contra orçamento explícito
de 48 KiB para lista+modal no host. O orçamento adicional é para os controles
do modal compartilhado e as etiquetas de tipo, sem ampliar o pool LVGL do P4.
Esses números com ponteiros de 64 bits não representam heap interno da placa.

Renders LVGL inspecionados: lista inteira, modal de evento, modal de tarefa,
teclado, segunda página e estado vazio. PNGs locais na pasta de teste:
`agenda-fullscreen.png`, `agenda-event-modal.png`, `agenda-task-modal.png`,
`agenda-keyboard-modal.png`, `agenda-page2-fullscreen.png` e
`agenda-empty-fullscreen.png`. Fixtures fictícias, sem dados da conta.
Os PNGs da revisão anterior foram preservados com seus nomes históricos.

### Build limpo e boot da revisão com modais

Mesma Waveshare P4 v1.3/revision 103, flash/PSRAM 32 MiB, base
`777abde7069c91cfb6aeb915891cf00bc249f9df` com working tree modificado.
Build novo em `firmware/build/agenda-modal-20261020`, IDF 5.5.4/esp32p4,
sem warnings. Os hashes efetivos de sdkconfig e partições continuam iguais
aos da tabela histórica. Dependências, C6, eFuses e pipeline não mudaram.
Hash instalado do C6 e inspeção completa de PCB/BOM/lote não foram obtidos.

O validador ELF passou: 63.752 B de SRAM estática, 125.768 B de BSS externa
(+80 B sobre a revisão inicial de UX) e redução estática interna de 133.696 B
frente à imagem anterior à campanha de memória. Pool útil LVGL no boot:
194.596 B, sem ampliação.

App-flash COM8/460800 gravou somente 3.511.168 B em `0x20000`, hash de dados
verificado, 58% da partição de 8 MiB livre. Boot restaurou um item e a conta,
`ready=1 pending=0 linked=1 sync_busy=0 sync_result=0`.

| Artefato da revisão com modais | SHA256 |
|---|---|
| P4 bin | `49474FE7B8D35D933927F0E2606F6F1A82E4B4C7F5683CC6965FCBA8FFBF3E14` |
| ELF | `FED4BDEE2A0787AC706E6F52E7814955BB6A0C049001C16A497FEF9B11AE15F1` |
| `sources.sha256`, 48 fontes públicos | `5500AE3F0EA9B72BCB1DAD5B751B8DD52517A0793B1DF73C57FECFFB7B9B03F4` |

Os comandos de reprodução são os mesmos da revisão inicial, substituindo
`agenda-ux-20261020` por `agenda-modal-20261020` no build e no sdkconfig.
`build.log`, `app-flash.log`, `boot-sanitized.log`, bin/ELF e manifesto ficam
locais e ignorados. Renders/testes host não equivalem a uma validação visual
ou tátil física; os gates de render, latência, energia e soak seguem abertos.

A sincronização automática da imagem com modais confirmou C6 3.0.6 e HTTP 200
em token/Calendar/Tasks, resultado 0. Mínimo interno 124.071 B, maior bloco após
79.872 B, pilha de rede mínima de 4.208 B e integridade 1. Nenhuma escrita nova
na conta foi necessária para a revisão visual.
A captura encerrou normalmente após 100 s e liberou COM8. Em 94,5 s, agenda
pronta com um item, `pending=0 linked=1 sync_busy=0 sync_result=0`; sem fault
capturado. Margem mínima do app_loop: 2.276 B em 8 KiB.

## 21/10/2026 — seis itens, controles no topo e autorização condicional

O cabeçalho agora reúne data local, filtros/contagens, criação e os ícones
de conectar/sincronizar. Seis linhas de 56 px ocupam o restante da área útil,
50% mais itens por página. Os 12 itens são alcançáveis em duas páginas;
paginação fica junto ao status, sem faixa de controles inferior. O tipo aparece
na informação secundária e no ícone, sem um badge redundante à direita.

Conectar fica oculto com a conta válida e as ações vizinhas se reposicionam.
A projeção interna inclui `google_auth_required`, sem alterar o registro
durável v1 ou mensagens JSON de integração. O resultado sanitizado -2001 é
emitido para HTTP 401 ou refresh HTTP 400 com `error=invalid_grant`; o app_loop
conserva a necessidade de autorização até um sync bem-sucedido. Não se usa
falha genérica de rede, quota/403 ou 5xx para pedir autorização de novo.
Fundamentação: [OAuth desktop](https://developers.google.com/identity/protocols/oauth2/native-app#errors)
e [erros Calendar](https://developers.google.com/workspace/calendar/api/guides/errors).
Nenhum body/erro bruto, token ou identificação da conta entra na projeção.

O status Google tem alvo de toque de 48 px e abre os detalhes no mesmo modal,
com último sincronismo e Liberar USB. Esse caminho mantém a confirmação
separada de commit do helper com o ícone conectar oculto. A janela física e sua
política de um comando continuam inalteradas. Tarefas têm modal de 400 px e
lembrete junto ao prazo; eventos conservam 480 px. Os botões primário e
secundário são reutilizados para conflitos e para o diagnóstico, sem novos
widgets de ação duplicados.

Passaram testes host de UI (100 lifecycles), projeção de necessidade de login,
ocultação/retorno do ícone, bloqueio de clique no controle oculto, expiração
de avisos, ações de conflito com o ID correto, modal Google/armar USB/voltar ao
editor, teclado e 12 itens sem perda na paginação. Heap host livre antes/depois:
2.070.888 B idênticos. Construção da lista: 31.344 B; modal lazy: 17.336 B;
agregado: 48.680 B, dentro dos tetos existentes de 40 KiB para lista e 48 KiB
para lista+modal, sem ampliar o pool do produto. São medidas com ponteiros de
64 bits, não SRAM dinâmica do P4.

Também passaram HTTP/vault/storage simulados, incluindo invalid_grant/401 e
não classificação de 403/5xx como login; cinco testes offline do helper,
conservando confirmação física separada; barramento com 8.000 snapshots
concorrentes. A autorização real da conta não foi revogada para esses testes.
Renders inspecionados ficam no diretório host, prefixo `agenda-compact-`,
sufixo `20261021.png`: tela cheia, autorização necessária, Google, tarefa,
evento, teclado, vazio e segunda página. Fixtures sem dados privados da conta.

### Build e bancada de 21/10

Mesma Waveshare P4 v1.3/revision 103, flash e PSRAM 32 MiB, base
`777abde7069c91cfb6aeb915891cf00bc249f9df` com working tree modificado.
Diretório novo `firmware/build/agenda-compact-20261021`, build iniciado do zero
e retomado após o ajuste do diagnóstico, antes de gravar; sem usar objetos
das entregas anteriores. IDF 5.5.4 e target esp32p4, sem warnings. Logs inicial
e final preservados como `build.log` e `build-final.log`.

Antes do app-flash, conferidos hashes idênticos de sdkconfig e partições e
layout ELF: 63.752 B de SRAM estática, 125.816 B de BSS externa (+48 B sobre
a versão modal de 20/10), mantendo a redução interna estática de 133.696 B.
Pool útil LVGL do boot segue em 194.596 B. Dependências, display, flash, C6
e eFuses inalterados; hash da imagem C6 instalada e inspeção completa de
PCB/BOM/lote não foram obtidos nesta campanha.

App-flash COM8/460800: 3.512.768 B em `0x20000`, hash verificado, 58% da
partição de 8 MiB livre. Boot restaurou um item e a conta vinculada, sem
salvamento pendente. C6 respondeu 3.0.6. Sync automático retornou HTTP 200
em token/Calendar/Tasks, resultado 0: mínimo interno 124.199 B, maior bloco
102.400 B, rede com 4.208 B livres/12 KiB, heap íntegro. Nenhum novo evento
ou tarefa foi criado e nenhuma autorização da conta foi revogada no hardware.

| Artefato da revisão compacta | SHA256 |
|---|---|
| P4 bin | `8E30EA9C7360BD07A2624300617609A8B38642A5FB85542EBA24D008D326CFFB` |
| ELF | `B273E71D73DC8C39427BA35FFB842CFD449F330950A27FCF500E0AF20A784678` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Tabela de partições | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto público de 48 fontes | `155B0175F5E96F59CB7B6188F4DE86BAA994FF8412EB77FC80A02244FB712549` |

Reproduzir com os comandos anteriores, substituindo o diretório de build e
o SDKCONFIG por `agenda-compact-20261021`. Bin/ELF, logs e manifesto são locais
e ignorados. A captura serial verifica boot/estado/HTTP; renders e testes host
não fecham ergonomia/touch físico, tearing, carga máxima, cortes ou soak.
O limite conhecido do teste antigo de filesystem cheio permanece registrado
em [AGENDA-MEMORY-VALIDATION.md](AGENDA-MEMORY-VALIDATION.md).
A captura de 100 s encerrou normalmente e liberou COM8. Em 93,5 s, agenda
pronta, um item, `pending=0 linked=1 sync_busy=0 sync_result=0`, sem fault
capturado. O app_loop manteve margem mínima de 2.276 B/8 KiB.

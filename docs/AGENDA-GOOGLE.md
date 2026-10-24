# Agenda, tarefas e lembretes — integração Google

## Escopo da primeira versão

A tela Agenda substitui os dados de exemplo por dados locais, acessíveis pelo
ícone de calendário no menu lateral. Permite criar e editar eventos com início
e fim, criar tarefas com ou sem prazo, concluir/reabrir tarefas, excluir com
confirmação e escolher lembretes: no horário, 5/15/30 minutos, uma hora ou um dia
antes. O fuso é o já configurado no painel. Datas impossíveis e horários
normalizados pelo sistema são recusados.

São **64 itens no total**, apresentados em até onze páginas de seis linhas
reutilizadas, com filtros Todos/Eventos/Tarefas e ordenação cronológica local.
A revisão visual está em [AGENDA-UX-VALIDATION.md](AGENDA-UX-VALIDATION.md).
Informações ocupam toda a largura; tocar em um item ou nas ações de criação
abre o formulário em um modal compartilhado com o restante do sistema.
Títulos têm até 48 bytes UTF-8. Esta versão usa a agenda
**principal** e a lista **padrão** do Google Tasks; importa os eventos dos
**mês corrente inteiro**, desde 00:00 do dia 1 até 00:00 do mês seguinte,
no fuso do painel, e as tarefas não concluídas. Eventos já ocorridos dentro
do mês continuam visíveis. As listas Google são percorridas em páginas de
dez itens, com os mesmos limites de corpo e parser. Somente itens com edição
local pendente exigem consulta individual de ETag. Após todas as páginas
de uma lista serem recebidas, ausências sem edição pendente são reconciliadas.
Eventos Google fora do mês e tarefas concluídas saem apenas da cópia local;
nenhuma exclusão no Google nasce dessa limpeza.
Não há seleção de múltiplas agendas/listas nesta versão.

Excesso de 64 itens, token de página acima de 256 bytes, mais de 65 páginas
ou payload/string maior que o limite gera
erro explícito, preservando dados locais e alterações ainda pendentes. Não há
truncamento silencioso ou promessa de espelhar toda uma conta com histórico.
Uma sincronização pode incorporar itens válidos antes de encontrar um erro;
a tela informa a falha e não declara a sincronização completa.

Eventos de dia inteiro são exibidos em modo somente leitura. O editor cria
eventos com horário; não cria séries recorrentes, participantes ou convites.
PATCH preserva os campos Google que o editor não administra. Alterar a categoria
de um item existente não é suportado pela UI.

**Google Tasks sincroniza somente a data do prazo.** A API não lê/escreve a
hora do prazo. A primeira importação usa 09:00 no fuso do painel; uma hora
escolhida localmente é preservada enquanto a data Google não mudar. O lembrete
da tarefa é local. Eventos usam os lembretes explícitos devolvidos pela API;
lembretes herdados das preferências da agenda não são inferidos.

## Notificações e relógio

`app_loop` avalia os lembretes com UTC confiável. O evento gera um item no
centro de notificações com o título do compromisso, um toast e a melodia de
alarmes escolhida em Configurações → Notificações. Preferências gerais/som
e volume continuam válidos. Catálogo em [SOUNDS-VALIDATION.md](SOUNDS-VALIDATION.md).
Não é um despertador com reprodução contínua, soneca ou escalonamento de áudio.

Cada lembrete é consumido uma vez por horário/antecedência, inclusive quando
as notificações estão silenciadas. Renomear o item não o rearma; mudar seu
horário ou antecedência rearma. Sem relógio confiável, nada dispara.
Recuperação aceita atraso de até 15 minutos; lembretes mais antigos e eventos
já encerrados expiram silenciosamente. Concluir/excluir uma tarefa cancela
seu lembrete; reabrir não repete um lembrete já consumido.

O sinal de lembrete consumido segue a persistência assíncrona. Um corte antes
da confirmação pode repetir um lembrete dentro da janela de recuperação;
não se promete entrega exatamente uma vez através de perda de energia.
O painel precisa estar ligado; retenção e deriva do RTC continuam sujeitas aos
gates físicos existentes.

## Estado, persistência e recursos

O domínio `agenda_service` usa C e tipos limitados, sem ESP-IDF, LVGL ou I/O.
Comandos de até 80 bytes entram na fila existente de 32 posições; saturação
retorna ocupado. Escritor único: `app_loop`. O status de cada comando inclui
seu identificador e resultado; revisão impede salvar uma edição desatualizada.

O FlashCoordinator é o único executor de filesystem/NVS. Agenda usa schema 1,
magic próprio, CRC do cabeçalho e payload, `agenda.0`/`agenda.1` em LittleFS,
temporário, flush, fsync, verificação e rename. A maior geração íntegra é
restaurada. Duas gerações corrompidas bloqueiam a agenda e preservam arquivos;
não existe autoformatação. Os arquivos de agenda são dados autoritativos do
usuário, separados do cache de providers. O antigo comando de formatação de
cache é recusado se houver agenda, alterações pendentes ou se sua ausência não
puder ser comprovada; não pode apagar silenciosamente os novos dados do usuário.
A gravação consolida cinco segundos
sem alterações e limita tentativas a uma por minuto. Até a confirmação, a UI
mostra salvamento pendente e o reinício de produto é recusado.

A agenda completa e as cópias normalizadas do storage/sync ficam em PSRAM
explicitamente, como exceção documentada para dados normalizados; não são TLS,
JSON nem corpos HTTP. Registro de domínio v1: 2.868 bytes; payload de persistência
cabe no buffer de 4 KiB já existente no coordenador, sem ampliar sua fila.
Projeção da agenda tem getter próprio, sem acrescentar a lista ao AppState
copiado em toda renderização. Há uma pequena projeção de trabalho e a view
LVGL de tamanho fixo. O teste host com ponteiros de 64 bits mediu 33.480 bytes
de alocação LVGL para a cena; o gerenciador tenta reservar 40 KiB, liberando
caches de cenas anteriores. Depois do aquecimento do teclado, 100 ciclos
mantiveram 2.070.840 bytes livres no pool host de 2 MiB. Isso não representa
o tamanho nem a margem do pool P4: a placa registrou 194.596 bytes totais,
incluindo a extensão PSRAM de 131.072 bytes já existente no bring-up.
O orçamento real precisa ser medido; limites de objetos e bytes não aprovam
o gate de memória.

O buffer normalizado de recepção de eventos do `app_loop` também é alocado
explicitamente em PSRAM, uma vez antes de criar a task. Continua pertencendo
somente ao `app_loop`; fila/pool do barramento e política de saturação não
mudam. Isso retira a cópia pública de evento da pilha de 8 KiB. A primeira
gravação mediu mínimo de 1.572 bytes livres, abaixo de 25%; o reteste da correção
por 100 s mediu 2.276 bytes (27,8%). Isso cobre esse boot, sem interação tátil;
não mede a pior carga de todos os comandos. Não houve aumento de pilha/Kconfig
nem uso de PSRAM por TLS/JSON.

O JSON Google tem corpo máximo de 12 KiB, 512 tokens e profundidade oito,
alocados explicitamente em SRAM interna. Não usa árvore JSON sem limite,
redirects, tokens em URLs ou logs de payload. A autorização privada USB é
limitada a 1.536 bytes e profundidade dois. Tokens ficam fora de eventos,
projeções e logs. A credencial OAuth é um pequeno registro NVS com duas gerações
e CRC, escrito somente pelo coordenador. No perfil de desenvolvimento existente
há retenção em NVS sem proteção física equivalente a produção; produção exige
NVS Encryption e Flash Encryption já autorizadas/configuradas pelo processo
próprio. Esta entrega não altera configurações de segurança ou eFuses.

## Sincronização

A cada cinco minutos online, com hora confiável e sem salvamento pendente,
`app_loop` tenta usar o mesmo executor HTTPS de clima/mercado/eWeLink/OTA.
Sincronizar permite uma tentativa manual. Não há segunda task HTTPS. Enquanto
uma sincronização está em curso, comandos de edição são recusados; navegação
continua funcionando. Duração prevista máxima do lote: 120 s, com timeout de
socket de até 10 s e verificação de deadline entre operações/dados. DNS e NTP
passam primeiro pelo caminho existente do executor.

Antes de iniciar TLS, a admissão exige 80 KiB de piso, envelope de 48 KiB
para TLS calibrado em bancada, workspace HTTP interno e 4 KiB transitórios de autorização;
maior bloco interno de pelo menos 32 KiB. Este envelope é conservador e
**ainda não foi qualificado com certificados Google em bancada**. Memória
insuficiente adia a sincronização com erro visível; não toma memória dos três
framebuffers. A evidência anterior de Sistema registrou apenas 54.187 bytes
internos livres. Portanto, não se deve prometer sincronização Google na carga
atual sem recuperar e medir a margem de SRAM.

Leituras precedem escritas. ETag/If-Match impedem sobrescrever uma alteração
externa concorrente. Conflitos ficam explícitos no item: “Usar Google” descarta
a intenção local; “Manter painel” permite tentar novamente com a versão remota
observada. Exclusão remota exige aceitar a versão Google, sem recriação
automática. Após escolher, aguardar salvar e sincronizar novamente.

Criações de eventos usam ID determinístico por placa, instância persistida
da agenda e ID local. Tarefas recebem um marcador nas notas na criação; se a
resposta de POST se perder, a leitura seguinte encontra o marcador antes de
tentar PATCH. Não há retry cego de POST. Não remover esse marcador até a
confirmação de criação/salvamento no painel. Metadados e sucessos parciais são
preservados para retomada. Conta Google não pode ser trocada reutilizando
identificadores da conta anterior; reautorizar aceita apenas o mesmo subject.

## Configurar o Google Cloud e vincular

1. No [Google Cloud Console](https://console.cloud.google.com/), crie um projeto
   para o NP2. Na biblioteca de APIs, habilite **Google Calendar API** e
   **Google Tasks API**.
2. Configure o Google Auth Platform: nome do app, público/contas autorizadas,
   e seu próprio usuário de teste quando o app estiver em teste. Adicione os
   escopos `openid`, `https://www.googleapis.com/auth/calendar.events` e
   `https://www.googleapis.com/auth/tasks`. A disponibilização pública pode
   exigir verificação Google; autorização de laboratório não equivale a isso.
3. Em Clients, crie **App para computador / Desktop app**. Baixe o JSON para
   `credentials/google-oauth-desktop.json` local, diretório já ignorado pelo Git.
   Não cole tokens, JSON OAuth ou senhas no chat, nos fontes ou em logs.
4. Com Python do ESP-IDF e `pyserial` disponível, execute na raiz:

   ```powershell
   python tools/link_google_agenda.py --client-secrets credentials/google-oauth-desktop.json --port COM8
   ```

   Ajuste a porta à USB Serial/JTAG do **P4**. Nenhum passo atualiza o C6.
5. Autorize Calendar e Tasks no navegador aberto pelo programa. O helper usa
   PKCE S256, state aleatório e callback em `127.0.0.1` com porta temporária;
   não grava tokens no computador nem imprime os dados de autorização.
6. Quando o terminal solicitar, toque no **ícone de conectar Google** no topo
   da Agenda, exibido quando a conta precisa de autorização, e
   pressione Enter no terminal. A janela física dura 60 s e aceita um comando.
   O helper transfere a credencial pela USB privada em blocos de 32 bytes,
   espaçados por 50 ms, para não saturar o RX de 256 bytes do driver.
   Ao receber o ACK, toque no **status Google > Liberar USB** e pressione Enter.
   Essa segunda janela permite consultar o commit mesmo com conectar oculto;
   um ACK de fila sozinho não é considerado vinculação concluída.
7. Aguarde as alterações locais serem salvas e use **Sincronizar**. O PC pode
   ser desconectado; renovação de acesso e consultas seguintes são do painel.

Se apenas a confirmação falhar, consulte novamente sem repetir OAuth:

```powershell
python tools/link_google_agenda.py --check-link --port COM8
```

Se o envio ficar sem ACK, faça também essa consulta antes de repetir OAuth:
a credencial pode já ter sido salva apesar de a resposta USB não chegar.
Abra a janela física quando solicitado. `GOOGLE_LINKED` só é aceito como
confirmação após o coordenador estar livre, com resultado de gravação OK e
credencial carregada. O helper não altera firmware nem aumenta buffers do P4.

Para renovar autorização expirada/revogada, repita com o botão **Reautorizar**
e a **mesma conta**. O app Google em modo Testing pode emitir refresh token
com vida limitada; não considerar isso falha de retenção do painel. A troca
de conta com política de migração dos dados não faz parte desta primeira UI.

O fluxo Google para TVs/dispositivos limitados não oferece Calendar/Tasks;
por isso a autorização inicial não usa um código de TV improvisado. Referências:
[OAuth desktop e PKCE](https://developers.google.com/identity/protocols/oauth2/native-app),
[escopos permitidos para TVs](https://developers.google.com/identity/protocols/oauth2/limited-input-device),
[Calendar events](https://developers.google.com/workspace/calendar/api/v3/reference/events),
[Google Tasks e limite da hora do prazo](https://developers.google.com/tasks/reference/rest/v1/tasks).

## Verificação e pendências

Testes reproduzíveis:

```powershell
./tools/run_agenda_host_test.ps1
./tools/run_google_agenda_sync_host_test.ps1
./tools/run_agenda_ui_host_test.ps1
./tools/run_app_event_bus_host_test.ps1
python tools/google_oauth_helper_host_test.py
```

Cobrem domínio, lembretes, revisão, datas/UTF-8, parser malformado e limites,
HTTP simulado com sucesso/falha/conflito/POST sem resposta, deduplicação de
polling, memória insuficiente, gerações/CRC e falhas de fsync/rename, tela LVGL
real e 100 ciclos de criação/destruição com teclado. O vault é testado com
provisionamento, reautorização da mesma conta, recusa de outra conta, falha de
commit e fallback CRC. O helper tem callback HTTP simulado, state inválido,
PKCE, escopos e ausência de tokens no stdout; esse teste não acessa Google.

**Vinculação realizada em 19/10/2026:** cliente Desktop local validado,
consentimento concluído no navegador e consulta USB `GOOGLE_STATUS` retornou
`GOOGLE_LINKED` pelo helper `--check-link`. A telemetria confirmou
`ready=1 items=0 pending=0 linked=1 sync_busy=0 sync_result=257`, com mínimo
de pilha de app_loop de 2276 bytes. `257` é `ESP_ERR_NO_MEM`: a admissão
de HTTPS continua bloqueada pela margem interna insuficiente. Não houve
sincronização real de eventos/tarefas nem teste de retenção após reboot.

O envio de credencial ficou sem ACK no vinculador; a consulta independente
confirmou posteriormente a persistência. O helper passou a espaçar blocos
de 32 bytes e a exigir uma nova janela física para a consulta, preservando
a política de um comando por toque. Sem ACK, orienta consultar antes de
repetir OAuth. A causa da perda de ACK em bancada permanece sem qualificação.
Cinco testes offline passaram, incluindo transferência em blocos, duas
janelas físicas, consulta isolada, recusa, escrita parcial e ausência de
falso sucesso a partir do ACK. Essas alterações são apenas no helper e nos
testes/docs; os artefatos P4 abaixo permanecem os já gravados. O manifesto
`sources.sha256` registra as ferramentas na versão da compilação; a revisão
posterior do vinculador está em `google-link-helper.sha256` no mesmo diretório.
Nenhuma credencial ou token foi incluído nesses manifestos ou nesta evidência.

**Atualização em 20/10/2026:** leitura real da conta passou após recuperar
SRAM e corrigir a pilha de rede. Token, Tasks e Calendar responderam HTTP 200;
um evento foi importado, outro criado na UI foi encontrado no Google após
reboot e a exclusão do teste retornou HTTP 204. O mínimo interno final foi
123.767 B e a pilha de 12 KiB ficou com 4.216 B livres. A conta foi restaurada
após reboot. A admissão e o
piso de 80 KiB continuam ativos. Evidências atualizadas e falha preexistente
do teste de filesystem cheio em
[AGENDA-MEMORY-VALIDATION.md](AGENDA-MEMORY-VALIDATION.md); os artefatos
abaixo registram a imagem anterior da agenda e não representam essa revisão.

**Pendentes:** matriz física de edição/notificação/som, cortes reais em LittleFS,
AP ausente, revogação Google, filas concorrentes e soak. Testes host de I/O não
qualificam durabilidade física do port LittleFS. Build/boot não fecham G2/G3/G4
nem qualificam esta integração como pronta para produção.

## Artefatos de bancada — 19/10/2026

Base Git `777abde7069c91cfb6aeb915891cf00bc249f9df` com alterações desta entrega
no working tree, descritor `777abde-dirty`. Build limpo inicial em diretório
novo `firmware/build/agenda-20261019`, SDKCONFIG separado, target `esp32p4`,
IDF 5.5.4 e toolchain riscv32-esp-elf 14.2.0/20260121. Incrementais finais
incluíram a correção de pilha, proteção contra formatar dados da agenda e
transporte HTTP por leitura/escrita explícita. O build final não apresentou
warnings de compilador; imagem `0x357200` = 3.502.592 B, com 58% livres no
menor slot de 8 MiB.

Configuração efetiva preservada: flash/PSRAM 32 MiB, P4 anterior a v3 com
revisão mínima 1.0, três framebuffers RGB565, rotação 180°/TRIPLE_PARTIAL,
reset54 ativo baixo, SDIO 4-bit GPIO18/19/14–17; sem auto-suspend, AES DMA ou
ESP_HOST_WIFI. Nenhuma dependência/lock, partição, C6, eFuse ou configuração
de segurança foi alterada. Esptool identificou P4 revisão v1.3; PCB/BOM não
foram reinspecionados. O hash do C6 instalado não foi obtido nesta entrega;
não inferir esse hash a partir da versão 3.0.6 fixada para o host.

| Artefato | SHA-256 |
| --- | --- |
| P4 bin final | `EDE9FE2E516751EB4748F2400A1695F9D03F3367C9D8ED7AAB5843620AFAA7C8` |
| P4 ELF final | `B04453C517E0F3AEBFD414B8059DCC438B180D41FBFD0C781F478EBA200080EF` |
| sdkconfig efetivo | `6E250C3E0E66AC3C86B4CDF4EF85DC05DA1AF899BB4F8180432F1BC864E692F4` |
| partition-table.bin | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| dependencies.lock | `6F605BE935C36425D451A65EC2A700AEF5878D9DAB7EA1000C2721199C552707` |
| Manifesto local dos fontes alterados | `7DEC927EF85FB71443483FEE26B264FCECFD7947324B4D74F649738569E5F9D0` |

Com ambiente IDF preparado, a partir de `firmware/`:

```powershell
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/agenda-20261019/sdkconfig -B build/agenda-20261019 build
python C:/esp/v5.5.4/esp-idf/tools/idf.py -DIDF_TARGET=esp32p4 -DSDKCONFIG=D:/Projetos/NP2/firmware/build/agenda-20261019/sdkconfig -B build/agenda-20261019 -p COM8 -b 460800 app-flash
```

Foi necessário executar a toolchain fora do sandbox Windows: nele, o GCC
RISC-V falhou ao iniciar com WinError 623/realocação de DLL. Não foi necessário
reinstalar ou alterar o SDK. A exportação usou o venv local existente em
`C:/Espressif/tools/python/v5.5.4/venv`, com `ESP_IDF_VERSION=5.5` e ROM ELFs
da instalação. Os caminhos acima são evidência desta estação, não preferência
compartilhada do editor.

Logs locais ignorados: `firmware/build/agenda-20261019-final-build.log`,
`firmware/build/agenda-20261019/flash.log`, `boot-sanitized.log`,
`boot-initial-sanitized.log`, `boot-stack-corrected-sanitized.log` e
`sources.sha256` no diretório desse build. A captura passiva em COM8 não envia
comandos e guarda apenas diagnóstico fixo de boot, recursos, enlace e agenda;
descarta nomes, dados de provedores e linhas brutas de rede. A primeira imagem
e sua medição de pilha foram preservadas para comparação, sem qualificá-las
como versão final.

**Resultado final:** `app-flash` em COM8/460800 gravou somente o aplicativo,
3.502.592 B em `0x20000`, com hash verificado pelo esptool e reset concluído.
Captura passiva pós-reset de 100 s concluída, porta liberada. ELF no boot
`b04453c51...`, correspondente ao artefato final da tabela. Agenda iniciou
`ready=1`, `items=0`, `pending=0`, `linked=0`; display informou RGB565,
rotação 180°, triple-partial e três FBs. PSRAM reconhecida: 33.554.432 B.
SW_AGGR negociou 15.872 B nas duas direções. Não houve linha de panic, assert
ou watchdog entre os diagnósticos monitorados; não se guardou log bruto de
rede e isso não constitui soak ou ensaio de falhas.

Na amostra final de 91 s: CPU 3%, cores 6%/0%, SRAM interna livre **47.839 B**,
maior bloco **23.552 B**, PSRAM livre 15.504.432 B. O mínimo de pilha do
`app_loop` ficou em **2.276 B** após a correção, acima de 25% dos 8 KiB nessa
carga de boot. **O piso interno de 80 KiB e a admissão TLS Google não foram
atendidos.** Portanto a integração Google está implementada e testada por
simulação, mas permanece sem sincronização real aprovada; recuperar SRAM e
medir o pico TLS é requisito antes de habilitar seu uso efetivo.

Não foi feita criação/edição por toque, disparo físico de lembrete, corte de
energia nem vinculação Google nesta bancada. A configuração Cloud e o
consentimento foram deixados para o responsável, conforme solicitado.

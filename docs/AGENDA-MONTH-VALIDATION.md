# Agenda mensal — 21/10/2026

A pedido do responsável, a Agenda passa a consultar o mês corrente inteiro
no fuso do painel, preservando eventos já ocorridos nesse mês. Cabeçalho
com mês/ano; seis linhas reutilizadas, até onze páginas e limite conjunto de
64 eventos/tarefas. Eventos de dia inteiro mantêm sua data na linha.
Continua usando agenda principal e lista padrão; sem seleção de outras agendas.

## Contratos e recursos

Janela calculada por calendário local e convertida para UTC: primeiro dia às
00:00 até primeiro dia do mês seguinte às 00:00, limite superior exclusivo.
Recorrências são expandidas. Google é lido em páginas de dez itens, incluindo
datas passadas no mês, com corpo de 12 KiB, 512 tokens, token de página de
256 bytes, até 65 páginas e deadline global de 120 s. Overflow é erro explícito.

Somente edições locais pendentes usam GET individual para ETag. Cada lista
é reconstruída com um snapshot de rollback de 15.244 B em PSRAM, preservando
IDs/revisões/lembretes dos itens observados. Falha em qualquer página restaura
a lista anterior; conclusão permite remover ausências limpas. Tarefas e
eventos continuam compartilhando o teto de 64 itens. Limpeza de cache nunca
apaga itens no Google e conserva edições pendentes.

Registro v2 de 15.244 B por payload; leitor valida e expande v1 de 2.868 B,
sem escrever/formatar durante leitura. CRC e duas gerações continuam. Leitor
antigo não entende v2; retorno a firmware antigo depende de cópia v1 íntegra.
Fila de flash recebe somente um ponteiro adicional e possui o snapshot
PSRAM de 15.244 B até a escrita; libera em erro de enqueue ou após consumo.
Há um pedido de Agenda em voo. UI e app_loop conservam seus donos anteriores.
HTTP, TLS, parser e credenciais continuam internos; buffers de paginação
acrescentam 2.820 B ao workspace interno admitido pelo mesmo piso de 80 KiB.

## Testes executados

- `tools/run_agenda_host_test.ps1`: limites de outubro, fevereiro bissexto,
  virada de ano e UTC já em novembro quando o painel ainda está em outubro;
  domínio cheio e codec passaram.
- `tools/run_google_agenda_sync_host_test.ps1`: 64 itens em sete páginas,
  eventos do início/fim do mês, aniversários de dia inteiro, cache cheio com
  substituição de item, 65º item rejeitado preservando cache, token escapado/
  repetido, falha em página posterior, CRUD/conflitos/ETag e falhas de
  autorização/memória passaram. Migração v1 com doze itens/IDs/ETags/lembretes,
  roundtrip v2 cheio, fsync/rename/CRC/fallback passaram. Vault passou.
  Produtor e consumidor reais do mailbox foram extraídos para teste de
  ownership, snapshot independente, fila ocupada e falhas de alocação/enqueue/
  escrita; todas as alocações retornaram ao baseline.
- `tools/run_agenda_ui_host_test.ps1`: todos os 64 IDs alcançáveis em onze
  páginas, filtros, comandos, modais, teclado, autorização e 100 lifecycles
  passaram. Lista 31.344 B + modal 17.336 B = 48.680 B no LVGL host; sem
  aumento do pool e sem perda após warm-up (2.070.888 B livres antes/depois).
- `tools/run_app_event_bus_host_test.ps1`: ownership, saturação e 8.000
  snapshots concorrentes passaram.

Renders `agenda-month-20261021.png` e `agenda-page2-month-20261021.png` em
`firmware/build/agenda-ui-host-20261019/` foram inspecionados. São fixtures,
não capturas da conta ou da tela física.

## Build e placa

Build iniciado em diretório novo `firmware/build/agenda-month-20261021`,
ESP-IDF 5.5.4, target explícito `esp32p4`. Verificação final sem warnings;
imagem 3.514.880 B (`0x35a200`), 58% livres no slot de 8 MiB. ELF: dados
estáticos internos 63.760 B e BSS externa 133.304 B. Aumento interno estático
de 8 B e externo de 7.488 B sobre a campanha compacta anterior. Credenciais
permanecem internas; pool, UI e projeções permanecem em `.ext_ram.bss`.

| Artefato | SHA256 |
|---|---|
| P4 bin | `6A4A678729C67F0CD98E5882A76008E949355F7646D3EAC0A7955736A0B5DEF4` |
| ELF | `3A579B16E801962835945F266457A7967FFB338BBB9310E2CE2E411799D85540` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições geradas | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |
| Manifesto de 49 fontes `sources.sha256` | `315213B7AA679B645CD5E7A3BCB6B0D821C78FD1667CD670A3308FCCBE7FE25B` |

Configuração e partições comparadas à imagem anterior antes da gravação;
sem mudanças. Logs locais ignorados: `build.log`, `build-final.log`,
`app-flash.log` e `boot-sanitized.log`. Base Git
`777abde7069c91cfb6aeb915891cf00bc249f9df` com alterações não commitadas.
Placa Waveshare ESP32-P4-WIFI6-Touch-LCD-7B v1.3, flash/PSRAM 32 MiB.
C6 não regravado; hash instalado continua desconhecido.

App-flash COM8/460800 gravou somente aplicação em `0x20000`; hash verificado.
Boot capturado por 100 s, porta liberada no fim: dois itens restaurados da
geração anterior e quatro após a leitura mensal real. Token, Tasks e Calendar
retornaram HTTP 200; resultado 0, mínimo interno 121.127 B, maior bloco
102.400 B, pilha de rede livre mínima 4.192 B, app_loop 2.280 B e integridade 1.
Em 94,3 s: `ready=1 items=4 pending=0 linked=1 sync_busy=0 sync_result=0`.
C6 reportou 3.0.6; sem fault capturado, criação/exclusão de dados de teste ou
nova autorização Google. A coleta sanitizada não exporta títulos nem IDs.

Os testes não encerram gates de tearing/toque físico, soak, power cut,
OTA/recovery ou a falha preexistente do harness de filesystem cheio.

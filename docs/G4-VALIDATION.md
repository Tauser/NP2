# G4 — protocolo físico de cache offline

Este protocolo fecha somente os cenários de cache/persistência descritos no
G4. Execute com a imagem P4 identificada no log, placa P4 v1.3, USB
Serial/JTAG e render ativo. Não grave nem atualize o C6.

## Regras

- Registre antes/depois a geração mostrada em `CACHE LOCAL`, o log bruto e
  qualquer artefato visual, reset ou WDT.
- A interface deve continuar utilizável. Se houver tela branca, tearing ou
  reset inesperado, interrompa a campanha e registre a falha; não formate
  `storage` como contorno.
- Quando a COM de log não expuser RX ao serviço USB, use os botões G4 da tela
  de diagnóstico. Eles só enfileiram a intenção no `FlashCoordinator`; não
  executam I/O na callback LVGL. Execute-os com a UI normal responsiva: a
  carga sintética contínua pode monopolizar a task LVGL e atrasar o worker de
  flash. O monitor pode permanecer somente-leitura para registrar `flash_coord`.
- O ensaio de filesystem cheio mostra `preparando storage`, o progresso
  aproximado em KiB e depois `testando rejeicao e limpando`; essa mensagem
  permanece até a proposta de cache e a limpeza concluírem. Se uma gravação, reset ou queda de
  energia interromper esse ensaio, o boot remove somente
  `/lfsdiag/full-probe.tmp`, `/lfsdiag/full-probe.tail` e propostas descartáveis;
  nunca remove as duas gerações de cache.
- A implementação faz `write + fsync` por bloco de até 16 KiB. Depois de
  `ENOSPC`, fecha e reabre o arquivo para recuperar o último checkpoint e
  reduz a escrita progressivamente até o tamanho do snapshot de domínio
  v1 (header de 24 B + payload de 25 B). Não executa `fsync` depois de uma
  escrita falhar: LittleFS pode retornar sucesso sem salvar o conteúdo de
  um arquivo marcado `LFS_F_ERRED`. Somente um erro preservado como
  `ENOSPC` conta como rejeição esperada; CRC, leitura, abertura, `fsync`, close
  ou limpeza falhos reprovam o ensaio. Ao final, o uso do volume deve retornar
  ao valor inicial.
- Preenchimento/propostas têm orçamento de 180 s verificado entre operações;
  chamadas individuais do filesystem e a limpeza podem ultrapassar esse
  instante. A expiração retorna timeout e executa limpeza; nunca conta como
  aprovação. O log informa a causa e a UI distingue `aprovado` de `FALHOU`.
- A ponte USB continua disponível onde o RX funcionar: `ARMAR REDE USB (60s)`,
  aguardar 1 s e enviar um único comando ASCII terminado por LF. O `OK` apenas
  confirma aceitação na fila; o resultado é a linha posterior de `flash_coord`.

## 1. Base de duas gerações e corrupção da mais nova

1. No diagnóstico, inicie a carga de render e execute `LFS 64x4K`.
2. Espere `LittleFS batch ... writes=64 verified=262144B`.
3. Arme a ponte USB e envie `CACHE_CORRUPT_NEWEST`.
4. Espere `cache newest-generation corruption ... completed`.
5. Reinicie o P4 e confirme que `CACHE LOCAL` mostra uma geração íntegra
   anterior à corrompida.

Passa se nenhum formato for solicitado e o reboot selecionar uma geração
válida. Este cenário já possui evidência em `BRINGUP-EVIDENCE.md`.

## 2. Filesystem cheio e limpeza

1. Com uma geração válida e UI normal responsiva (carga sintética pausada),
   toque `G4: FS CHEIO`
   (ou arme a ponte e envie `CACHE_FULL_PROBE`).
2. Aguarde a conclusão; a operação preenche somente os temporários de prova,
   executa propostas descartáveis com o mesmo `write + fsync` de uma proposta
   de cache até a primeira rejeição, mas não faz `rename` sobre uma geração
   ativa. Ela então remove todos os temporários e qualquer `cache.tmp` parcial.
3. Espere `cache full-filesystem probe ... preserved generation`.
   O log anterior deve mostrar `full probe proposal` com `errno=28` (ENOSPC
   no ESP-IDF) e `full probe cleanup` com espaço recuperado. A UI só mostra
   `aprovado: ESP_OK` quando esses critérios e a preservação do cache passam.
4. Confirme na tela que a geração anterior continua íntegra, sem formatar
   `storage`; reinicie e repita a confirmação.

Passa se a geração não mudar, `ENOSPC` for a causa da rejeição, todos os
temporários forem removidos, o espaço inicial for recuperado e não houver
reset ou artefato visual. Esse teste pode escrever quase toda a partição de
9 MiB e só deve ser executado de forma observada na bancada.

### Regressão host do algoritmo LittleFS

Execute na raiz do repositório, com GCC host e o componente resolvido:

```powershell
./tools/run_cache_full_probe_host_test.ps1
```

O runner compila as rotinas reais do coordenador com o núcleo LittleFS
fixado pelo projeto, sobre flash NOR simulada de 9 MiB. Cobre os dois
tamanhos de cache, repetição, remontagem, EIO, limpeza e timeout. Isso valida
o algoritmo de preenchimento/persistência e erros, mas não mede duração,
latência LVGL, efeitos de flash físico ou corte elétrico da placa.

## 3. Corte entre `fsync` e `rename`

1. Garanta cache válido e anote a geração.
2. Toque `G4: CORTE PRE-RENAME` (ou arme a ponte e envie
   `CACHE_CUT_BEFORE_RENAME`).
3. Ao aparecer no painel `G4: DESLIGUE AGORA! antes rename (10s)`, corte a
   alimentação física do P4 dentro de 10 s. O mesmo marco também é emitido
   no serial como `POWER_CUT_NOW before rename; window=10000ms`.
4. Restaure a alimentação, capture o boot e confira o card de cache.

Passa se a geração anterior ainda for íntegra. Sem corte, a operação expira,
o painel mostra `G4 corte: NAO VALIDADO (sem corte em 10s)` e o temporário
é apagado; esse resultado não conta como teste físico.

## 4. Corte após `rename`

1. Repita a preparação e anote a geração atual.
2. Toque `G4: CORTE POS-RENAME` (ou arme a ponte e envie
   `CACHE_CUT_AFTER_RENAME`).
3. Ao aparecer no painel `G4: DESLIGUE AGORA! apos rename (10s)`, corte a
   alimentação física dentro de 10 s e então restaure-a. O mesmo marco também
   é emitido no serial como `POWER_CUT_NOW after rename; window=10000ms`.
4. Capture o boot e o card de cache.

Passa se o boot selecionar a geração anterior ou a nova, mas sempre uma
geração íntegra; CRC inválido apresentado como dado, format automático, panic,
WDT ou perda de responsividade reprovam o cenário.

## Fechamento parcial de persistência

Anexe a evidência de cada cenário a `BRINGUP-EVIDENCE.md` com data, imagem P4,
configuração efetiva, comando, geração antes/depois, logs e observação visual.
Os testes de cache não fecham G4 por si: o gate ainda exige dados de domínio
offline, estados stale/erro, contratos de provider limitados e suas provas.

## 5. Snapshot de domínio após reboot

Brasília-DF é a localidade de produto fixada para o adapter de clima
(`-15.793889`, `-47.882778`). Com IP e hora válidos, arme a manutenção e envie
`REFRESH_OFFLINE_DATA` ou toque `G4: ATUALIZAR DADOS BRASILIA`; o worker
HTTPS único consulta clima e BTC/USD sequencialmente, com corpo limitado a
768 B e sem token. Ele não possui polling automático.

Após `NTP=ESP_OK`, o serviço de hora deve deixar o rodapé em `hora confiavel`.
Uma atualização bem-sucedida deve mostrar `ha 0 min` nos dois cards. Se os
valores forem persistidos, mas os cards ainda mostrarem `hora nao confiavel`,
registre como falha: não é resultado aceitável de G4. Sem uma sincronização
válida após reboot, essa indicação é esperada e os dados locais são mantidos.

Em bancada, execute duas atualizações completas, reinicie o P4 com a rede
indisponível e confirme nos cards que clima e BTC/USD mostram `DADO LOCAL`
ou `DADO LOCAL ANTIGO`, nunca JSON, URL ou erro de transporte. O cache
técnico criado pelos ensaios `LFS 64x4K` não é um snapshot de domínio e, por
projeto, não deve ser exibido como se fosse dado de clima ou mercado.

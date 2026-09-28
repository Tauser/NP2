# Settings — fechamento funcional e aceite físico

## 2026-09-28 — Modo noturno

Horário local das 22:00 às 06:00, com brilho efetivo limitado a 15%. O slider
conserva a preferência diurna; desligar o modo ou sair do horário restaura
esse valor. Um brilho inferior a 15% não é aumentado. Volume e notificações
não são alterados. Sem horário confiável, o efeito aguarda sincronização.

A preferência é salva pelo worker/coordenador de flash no perfil de
controles, sem alterar o schema offline. Leitura antiga de dois bytes
preserva brilho/volume e inicia Modo noturno desabilitado. Os slots antigos
não são apagados nem migrados automaticamente no boot.

`tools/run_settings_policy_host_test.ps1` passou com GCC/`-Werror`: horário,
tempo desconhecido, limites de brilho, payload antigo/atual e payload
inválido. A regressão de fuso também passou. `idf.py build` passou para
P4/IDF 5.5.4, aplicação `0x2a6260` B, 67% livres no slot de 8 MiB.

Brilho, volume, notificações e usabilidade do teclado foram aprovados pelo
operador. O layout/fuso da revisão anterior e o Modo noturno desta revisão
precisam de novo flash manual e teste. Não há nova evidência de boot/reboot,
temperatura ou persistência física nesta entrega.

## 2026-09-28 — Sistema

- Versão real obtida do descritor da imagem. Temperatura interna do chip
  obtida pelo driver IDF a cada 5 s no worker de controles; ausência/falha
  aparece como indisponível, sem temperatura ambiente sintética.
- Reiniciar painel abre confirmação lazy. Cancelar/X/sair não enfileiram
  comando. O coordenador revalida preferências, gravações, sessão/journal
  OTA e estado de boot antes de executar o reinício em sua própria task.
  Uma recusa mantém a confirmação aberta, ou publica feedback se surgir
  depois de enfileirar. Nenhuma leitura de OTA-data ocorre no callback LVGL.
- Atualização sem função foi substituída pela indicação de manutenção;
  nenhuma OTA é liberada pela tela enquanto G5 permanecer aberto.
- Admissão do reinício passou no teste host; regressões de fuso e senha
  também passaram. Temperatura/reinício dependem de validação física.
- `idf.py build` passou no P4/IDF 5.5.4: aplicação `0x2a9480` B, 67% livres,
  sem novos warnings de compilação. Modo noturno foi versionado em `3358453`.

## Aceite após flash manual

- Conferir o fuso em cidades com offsets diferentes, reabrir o modal e
  reiniciar. Região/busca/lista devem permanecer alinhadas.
- Ativar Modo noturno com hora confiável antes/depois dos limites 22:00/06:00.
  Conferir brilho de 10%, 60% e 100%, desativar e observar a restauração.
- Reiniciar e conferir retenção de preferências. Sem hora confiável, o painel
  deve manter o brilho escolhido e informar espera de sincronização.
- Repetir os fluxos Wi-Fi do roteiro em `WIFI-SETTINGS-VALIDATION.md`.
- Sistema: comparar versão com o descritor do build; conferir que a
  temperatura é do chip e que falha de sensor mostra indisponibilidade.
- Reiniciar: cancelar e fechar devem preservar a execução atual. Confirmar
  após salvar deve reiniciar sem perder preferências. Durante gravação ou
  OTA/boot ainda não confirmado, o pedido deve ser recusado e permitir retry.

Registrar unidade/BOM, commit, hashes P4/C6, configuração efetiva, data e
log sanitizado. Build/teste host não comprovam estabilidade física de Settings.

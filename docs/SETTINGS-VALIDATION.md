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

## Aceite após flash manual

- Conferir o fuso em cidades com offsets diferentes, reabrir o modal e
  reiniciar. Região/busca/lista devem permanecer alinhadas.
- Ativar Modo noturno com hora confiável antes/depois dos limites 22:00/06:00.
  Conferir brilho de 10%, 60% e 100%, desativar e observar a restauração.
- Reiniciar e conferir retenção de preferências. Sem hora confiável, o painel
  deve manter o brilho escolhido e informar espera de sincronização.
- Repetir os fluxos Wi-Fi do roteiro em `WIFI-SETTINGS-VALIDATION.md`.

Registrar unidade/BOM, commit, hashes P4/C6, configuração efetiva, data e
log sanitizado. Build/teste host não comprovam estabilidade física de Settings.

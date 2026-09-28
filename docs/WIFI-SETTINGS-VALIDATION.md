# Settings Wi-Fi — validação incremental

## 2026-09-28 — Status e seleção

- Base: `74f543e`. `connected_ssid` público vem de `WIFI_EVENT_STA_CONNECTED`,
  passa pelo snapshot do serviço e pelo escritor único `app_loop` até Settings.
  Desconexão, retry, forget e perda/recuperação de SDIO limpam a associação.
- O card não presume internet, IP ou RSSI da conexão. RSSI nas redes disponíveis
  continua sendo somente o valor medido no scan.
- Seleção começa vazia e habilita Conectar apenas após toque; um novo scan
  preserva o SSID escolhido ou invalida a seleção se ele saiu da lista visível.
- Gerenciar redes solicita scan no modal atual. Não abre mais a tela legada.
- `idf.py clean build` passou para `esp32p4`, IDF 5.5.4: aplicação
  `0x2a0fb0` B, 67% livres no slot de 8 MiB. Configuração efetiva conferida:
  RGB565, três FB, reset54 ativo baixo e auto-suspend desativado.
- Flash manual por preferência do responsável. Sem captura de boot, hash C6
  instalado ou resultado novo de bancada nesta etapa.

## Roteiro após flash manual

Registrar data, unidade/BOM, commit, SHA-256 P4/C6, configuração efetiva e log
serial sem segredos. Baseline: P4 v1.3, flash/PSRAM 32 MiB, IDF 5.5.4,
ESP-Hosted 3.0.6 nos dois chips, RPC v2/SDIO SW_AGGR, GPIO54 ativo baixo,
RGB565/180°/TRIPLE_PARTIAL/três framebuffers, auto-suspend desativado.

1. Abrir Settings → Wi-Fi; repetir abrir/fechar e trocar entre outros modais.
   Não deve haver panic, WDT ou mais de um modal principal de Settings.
2. Conectar começa desabilitado. Tocar em cada rede seleciona somente aquela
   linha e habilita Conectar rede. Fazer scan novamente: preservar por SSID,
   sem trocar silenciosamente a rede escolhida por causa da ordem da lista.
3. Conferir SSID com o AP real; desligar AP e verificar que Status deixa de
   apresentar associação antiga. Com IP e sem internet, não afirmar internet.
4. Gerenciar redes e lupa usam busca limitada, sem abrir o setup legado.

Build não comprova ausência de crash, estabilidade gráfica ou gate de rede.
Os resultados físicos acima permanecem pendentes do operador.

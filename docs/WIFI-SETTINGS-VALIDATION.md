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
- Commit: `305c5ac`; SHA-256 P4 do build limpo anterior ao commit:
  `49E9F57397C4B81AFB82B1EF21C19DC656D83846AFE2B5286CF5282861C3494F`.
  O único warning C de aplicação no build limpo foi o `const` pré-existente
  em `device_control_service.c` na chamada de `esp_codec_dev_open`.

## 2026-09-28 — Diálogo compacto de senha

- Diálogo filho lazy de 640 × 500, com um teclado matricial sem textarea.
  Abertura e liberação anterior ocorrem após o callback de toque.
- Senha somente no serviço; Mostrar/Ocultar usa caracteres transitórios no
  desenho (autorização do responsável, ADR-040). Cancelar/fechar/sair/submeter
  zeram o buffer e revogam a exibição. Rede aberta exige senha vazia;
  protegida exige 8–63 caracteres. Falha de envio não anuncia conexão pronta.
- `tools/run_provisioning_touch_host_test.ps1` compila o serviço real com
  shims de plataforma e testa sessão, duplicação, caracteres inválidos,
  limites, máscara/revelação, backspace, cancelamento, rede aberta, submissão
  e zeragem após mailbox indisponível. Passou com GCC e `-Werror`.
- `idf.py build` passou com IDF 5.5.4/`esp32p4`, aplicação `0x2a1b80` B,
  67% livres; sem novos warnings de compilação. SHA-256 P4:
  `99A5CFE8701CFCD305258676DBF0AE5F5F2F923FB35E2D07048DEB0C0E414279`.

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
5. Selecionar rede protegida → Conectar rede: verificar SSID, teclado, letras
   maiúsculas/minúsculas, símbolos e espaço, apagar, máscara e Mostrar/Ocultar.
   Com 7 caracteres Conectar deve estar desabilitado; com 8 e 63 habilitado;
   a 64ª tecla deve ser recusada. Cancelar e X devem voltar ao Wi-Fi e uma
   nova entrada começa vazia/mascarada. Repetir 100 ciclos para avaliar heap.
6. Submeter senha incorreta e correta; UI continua operável e só apresenta
   associação/SSID quando o rádio confirmar. Testar rede aberta sem senha.
   Confirmar ausência de segredo em logs e projeções; não exportar dump bruto
   enquanto existirem credenciais ou pixels da revelação.

Build não comprova ausência de crash, estabilidade gráfica ou gate de rede.
Os resultados físicos acima permanecem pendentes do operador.

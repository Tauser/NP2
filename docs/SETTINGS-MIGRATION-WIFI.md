# Migração Wi-Fi — 2026-09-28

## Implementação e revisão prévia

Revisados `wifi_setup_view.c/.h`, `provisioning_service.c/.h`,
`connectivity_diagnostic.c/.h`, `app_state.c/.h`, o consumidor
`network_validation_service`, a projeção, `product_ui`, o teclado e o diálogo
de senha existentes. O backend real é `connectivity_diagnostic`; não existe
necessidade de outro serviço. `wifi_setup_view` ainda é chamado por
`diagnostic_ui` para provisionamento inicial/manutenção e continua compilado.

Os arquivos pedidos `np_settings_wifi.c/.h` já existiam como modal. Foram
ampliados com uma cena própria e handles separados; o modal legado continua
disponível pela Settings anterior. Não houve remoção de Settings, onboarding,
nem alteração de CMake: os arquivos já estavam registrados.

## Tela e navegação

Preferências → Wi-Fi → Voltar → Preferências. Header compartilhado preservado,
sem redesenhar Home. Painel esquerdo 600×496, direito 360×496, na área abaixo
do header de 1024×600, Dark Graphite e tokens reais do produto. Sem blur,
imagens ou sombras pesadas. Títulos, rede atual, quatro linhas disponíveis,
detalhes, ações e ícones seguem a composição do modelo.

Adaptações funcionais ao modelo:

- Switch liga/desliga omitido: não existe contrato para essa função.
- Lista contém somente SSID, cadeado quando protegida e ícone por intensidade,
  conforme a decisão anterior do produto. RSSI fica nos detalhes.
- Segurança é aberta/protegida; o snapshot atual não identifica WPA2/WPA3.
- RSSI real vem da última busca, explicitamente identificado como tal.
- IP/gateway reais aparecem somente na associação atual e com IP obtido.
  Campos sem fonte disponível mostram indisponibilidade.
- `online` significa DHCP/IP, sem declarar internet verificada.
- Conectar fica desabilitado para a associação atual ou sem resultado
  selecionado. Esquecer atua apenas na associação atual e exige confirmação.
- Adicionar rede é real: SSID manual, escolha aberta/WPA2 e depois o diálogo
  de senha existente. Nenhuma credencial é persistida diretamente pela UI.

As quatro rows são reutilizadas em até dez páginas para as 40 entradas
limitadas do serviço. Abrir a tela não cria 40 rows. A lupa apenas solicita
scan via `product_ui`; o worker faz o scan bloqueante fora da task LVGL.
Resultado segue serviço → app_loop → projection → product_ui → UI. Falha
de scan permite nova tentativa e informa o erro sem apagar a última lista.

## Senha e lifecycle

Diálogo pequeno acima do teclado global, com olho, Cancelar e Conectar.
Textarea de uma linha/password mode mantém apenas caracteres de máscara;
inserções diretas no widget são recusadas. Teclas seguem o contrato privado
do provisioning, cujo buffer é o único dono da senha. O desenho imediato de
glyphs para revelação temporária preserva a autorização anterior do produto.
Nenhuma senha entra no texto de widget, state/projection, toast, diagnóstico
ou log. O SSID manual é público e pode usar textarea convencional.

Submit/cancel/saída limpam a sessão e escondem teclado/target. O provisioning
já zera seus buffers após submit, incluindo falha da mailbox; a UI pede nova
entrada nesse caso. Fechar Settings Wi-Fi também descarta diálogos pendentes.
Saída cancela aberturas adiadas antes da destruição. O vínculo do SSID é
devolvido ao pool do teclado ao destruir seu input. A reconciliação de foco
adiada é cancelada na limpeza do target.

A cena é criada lazy ao entrar e liberada ao sair; apenas o cache de Sistema
preexistente continua residente. Não há timers novos. Os callbacks são
instalados uma vez por árvore, com bind idempotente das rows. Instrumentação
registra tempo de construção e número de objetos para a bancada.

## Recursos e validação

- Teste com LVGL 9.5.0 real e fontes reais: **98 objetos**, incluindo header,
  drawer fechado e paginação; teclado global separado. Número permanece
  constante ao percorrer as dez páginas.
- **100 ciclos** de construção/seleção/paginação/destruição, rede atual fora
  do scan, dados IP condicionais, máscara de 63 caracteres, revelação,
  tentativa de inserir texto diretamente, submit, teclado aberto na saída,
  SSID manual e liberação dos bindings: passaram no host.
- Teste existente do provisioning: rede protegida/aberta, 7/8/63 caracteres,
  rejeição de caracteres, olho, backspace, cancelamento, falha da mailbox e
  zeroização: passou. Esse teste usa o serviço real com I/O de plataforma
  substituído; não simula associação/DHCP/vault reais.
- Fontes regeneradas com `lv_font_conv` local: 24 px 103→106 glyphs, 48 px
  16→20. Nenhum glyph anterior removido; altura e baseline preservados.
  Render RGB565 da cena em host conferido visualmente. Dados de teste não
  foram incluídos no firmware de produto.
- Map P4: `s_ui` 0x16a0 (5792 bytes), +1864 bytes sobre Notificações.
  Projeção e snapshot de conectividade ganham 32 bytes cada. Aumento estático
  total dessas estruturas: 1928 bytes. Heap LVGL e stacks reais exigem medição
  na placa; o tamanho das estruturas em host não substitui essa medição.
- Build limpo P4/ESP-IDF 5.5.4 e build habitual obrigatórios antes dos commits.
  WDT de 5 s, RGB565/TRIPLE_PARTIAL, três framebuffers, locks e partições
  permanecem iguais. Schema offline e políticas do vault também.

Comandos reproduzíveis, na raiz:

```powershell
./tools/run_provisioning_touch_host_test.ps1
./tools/run_settings_wifi_ui_host_test.ps1
```

O teste de UI utiliza GCC/MinGW e CMake/Ninja locais, e o LVGL já resolvido
em `managed_components`. Seus arquivos gerados e render de teste ficam em
TEMP, fora do Git. O build de firmware deve usar o ambiente IDF 5.5.4 e target
explícito `esp32p4`.

## Aceite físico pendente — flash manual

Não foi realizado flash nem teste de rádio nesta entrega. Registrar unidade,
commit, hashes P4/C6, configuração e logs sem segredos para:

1. Entrar/sair repetidamente pelo hub, comparar com o modelo em 1024×600.
2. Scan com APs reais, lista vazia, mais de quatro resultados, nomes longos,
   redes abertas/protegidas e novas tentativas após falha.
3. Senha curta/limite, símbolos/caps, olho, Cancelar, submit e falha de senha.
4. Conexão/DHCP, sucesso, falha, troca de AP, perda de IP e gateway ausente.
5. Rede manual aberta/WPA2; fechar e navegar com teclado aberto, inclusive
   pela Settings legada; teclado deve ficar acessível e sem target antigo.
6. Esquecer com Cancelar/Confirmar e reboot; verificar remoção/restauração
   de acordo com o estado real do vault protegido. Aceitar mailbox não é
   comprovação de remoção durável. O backend preexistente ainda pode limpar
   a configuração RAM quando o pedido de remoção do vault é recusado;
   validar esse caso antes de declarar o fluxo durável concluído.
7. Repetir navegação/scan com instrumentação de heap interno/PSRAM, maior
   bloco, stacks, tempo da task LVGL e logs WDT. Teste host/build/foto não
   comprovam estabilidade física, persistência após reboot ou ausência de WDT.

## Artefato validado em 28/09/2026

Build P4 limpo em `build/wifi-final-clean` e `idf.py -D IDF_TARGET=esp32p4 reconfigure build` concluídos. Sem warnings de compilação relevantes. Binário normal: `0x2b1410` bytes, partição de aplicação `0x800000`, 66% livres. SHA256: `516c11ae51dc29be2e4c1ee30e03b52d4858b09ebdc5bc1d9681f153b54efb56`. Descrição de origem: `9182361-dirty`, incluindo a implementação de UI desta entrega. Flash permanece manual.

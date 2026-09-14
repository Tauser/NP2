# UI do NP2 no EEZ Studio

O arquivo `NP2.eez-project` descreve a interface visual de 1024 x 600. Ele usa
LVGL 9.5 sem EEZ Flow: a aplicação continua dona da lógica, dos serviços e da
persistência.

## Organização

- `NP2.eez-project`: fonte visual editável no EEZ Studio.
- `firmware/main/ui_generated/`: saída gerada pelo EEZ. Permanece plana e não deve receber edição ou movimentação manual, pois será regravada ao gerar o projeto.
- `firmware/main/ui/bridge/`: integração manual entre o `AppState` e os widgets gerados.
- `firmware/main/ui/legacy/`: dashboard e diagnóstico antigos; `legacy/fonts/` contém apenas a fonte Montserrat 14 ainda usada por essa tela.

## Componentes reutilizáveis

O projeto contém dois User Widgets prontos para páginas novas:

- `NP_Header` (1024 x 66): menu, configurações, Wi-Fi, Bluetooth, alertas e divisor inferior.
- `NP_SideDrawer` (1024 x 600): scrim, gaveta lateral e atalhos de navegação.

A Home já usa uma instância de cada componente: `home_app_header` e
`home_app_drawer`. Ao gerar o código novamente no EEZ, os identificadores
internos passam a pertencer às instâncias; atualize o bridge de firmware contra
essa saída antes de compilar e gravar uma nova imagem.

## Fluxo inicial

1. A primeira tela é `Boot`.
2. O firmware apresenta essa tela antes de iniciar I/O de inicialização.
3. Cada etapa concluída atualiza somente o texto de estado e os cinco segmentos:
   `Display`, `Armazenamento`, `Rede`, `Hora` e `Dados`.
4. Após um prazo limitado, o firmware carrega `Home`, mesmo sem rede ou hora
   confiável. Dados em cache permanecem visíveis com o respectivo estado de
   desatualização.

`Dados` não bloqueia a entrada em `Home`: clima e Bitcoin chegam depois, no
fundo, pelo fluxo de requests já serializado.

## Dados que alimentam a Home

| Elemento | Fonte do firmware | Comportamento sem dado atual |
| --- | --- | --- |
| Hora e data | `TimeService` e `AppState.time_trusted` | Mostrar hora anterior ou `--:--`; indicar relógio não sincronizado. |
| Wi-Fi e Bluetooth | projeção de conectividade em `AppState` | Alterar somente a cor dos ícones: verde conectado, vermelho com falha e cinza indisponível; nunca expor SSID/senha na tela inicial. |
| Temperatura e condição | cache e resposta do provedor de clima | Manter último valor e informar que está desatualizado. |
| BTC/USD e variação | cache e resposta do provedor de mercado | Manter último valor e informar que está desatualizado. |
| Vento, umidade, sensação e UV | modelo visual; ainda sem contrato completo de provider | Não publicar o valor demonstrativo no firmware. Ocultar ou usar `--` até existir dado validado. |
| Dólar e Ibovespa | modelo visual; ainda sem provider aprovado | Não publicar a cotação demonstrativa no firmware. Ocultar ou usar `--` até existir dado validado. |

Clima e mercado usam um ponto de estado: verde para dado atual, vermelho para
falha/expiração e cinza para carregamento ou ausência de fonte. A Home não
mostra frases de sincronização.

## Header e menu lateral

O header contém somente áreas de toque de 44 x 44 px para menu, configurações,
Wi-Fi, Bluetooth e alertas. Os ícones visíveis são menores, mas toda a área é
clicável.

O menu lateral `home_side_drawer` inicia fechado em `x = -96`. Ao tocar em
`home_menu_button`, o apresentador deve animá-lo até `x = 0` em 120–180 ms e
mostrar `home_drawer_scrim`. Fechar faz a animação inversa e esconde o scrim.
O drawer não dispara rede, persistência ou construção de tela; ele apenas emite
uma intenção de navegação.

## Regra de atualização

- Somente a task LVGL altera widgets.
- O apresentador recebe uma cópia coerente de `AppState` e compara texto,
  cor e visibilidade antes de tocar em um objeto LVGL.
- O relógio atualiza os dígitos que mudaram; clima, rede e mercado atualizam
  apenas quando seu valor ou seu estado muda.
- A UI não inicia HTTP, NTP, NVS, cache ou drivers.

## Próxima integração

Após revisar o layout no EEZ Studio, gere o código em
`firmware/main/ui_generated`. A integração deve chamar `ui_init()` e qualquer
alteração de tela dentro do contexto já protegido da task LVGL. As fontes usadas
pela Home são geradas pelo EEZ em `firmware/main/ui_generated`; não adicione
cópias manuais dessas fontes ao `main`.

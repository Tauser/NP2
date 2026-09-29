# Dados públicos de Wi-Fi — 2026-09-28

O serviço existente `connectivity_diagnostic` mantém a propriedade do rádio,
scan, mailbox de credenciais, reconexão e vault. Não foi criado outro backend.
O `app_loop` continua sendo o único escritor da projeção.

`IP_EVENT_STA_GOT_IP` agora copia IPv4 e gateway para dois campos públicos de
16 bytes. Endereço zero permanece indisponível. Esses campos são limpos na
associação, troca de configuração, disconnect, falha de transporte, recovery,
forget e perda de IP. Em `STA_LOST_IP`, o deadline de DHCP existente volta a
valer para uma associação ainda ativa. A inscrição de eventos IP cobre esse
evento e é removida simetricamente durante o lifecycle do rádio.

Fluxo: evento do driver → snapshot protegido do serviço → `app_loop` →
`app_network_projection_t` → `product_ui` → tela. Nenhum acesso ao netif,
driver ou NVS foi acrescentado à UI. Não há alteração de payload offline,
schema, persistência, políticas de TLS, C6 ou WDT. Os campos não são eventos
de protocolo; a revisão coalescível da projeção existente continua suficiente.

Um scan que falha agora termina em `SCAN_COMPLETE` com o erro real, em vez de
permanecer em `SCANNING` e impedir nova tentativa pela interface. A última
lista válida continua disponível; a UI deve indicar que a busca falhou.

`online` continua significando IP obtido. Não comprova acesso à internet.
RSSI e segurança publicados continuam sendo os dados da última busca:
intensidade em dBm e aberta/protegida, sem inferir WPA2/WPA3. Nenhuma senha
entra nesses snapshots.

Build P4 obrigatório antes do commit. A captura real de DHCP, perda de IP,
falha de scan e recuperação exige flash manual e evidência de bancada.

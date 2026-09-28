# Perfil e Preferências — primeira fase

Branch: `codex/settings-migration`. Base: `45a4189`.

## Escopo entregue

- `np_profile.c/.h`: saudação sempre visível, avatar sem imagem, Perfil
  principal, Conta (Nome/Avatar), Preferências pessoais (Tela inicial/Abrir
  preferências). Sem switch de saudação ou seleção de perfil padrão.
- `np_preferences.c/.h`: cinco linhas com ícone de fonte, título, subtítulo
  e chevron. Nenhum controle de configuração no hub.
- `product_ui.c`: Home → drawer → Preferências ↔ Perfil; Perfil → Home;
  Preferências → Settings anterior → Preferências ou Home.
- `CMakeLists.txt`: registro das duas novas fontes. Settings anterior mantida.

Não há identidade real no estado atual. Nome não informado, avatar `--`,
saudação `Olá!`, localização omitida. Editar/Nome/Avatar mostram a pendência
da fase de identidade. Não há edição ou persistência de nome nesta entrega.
Tela inicial informa Home e permite voltar para ela; não cria preferência
de inicialização. Os cinco itens abrem o mesmo fallback Settings: o usuário
escolhe o controle/modal desejado nessa tela existente.

## Recursos

Contagem estática aproximada, incluindo root e header/drawer: Perfil 66
objetos LVGL, Preferências 71 após o ajuste visual. Teclado, feedback e a tela ativa global não
entram nesses números; são recursos compartilhados existentes. Sem árvores
das futuras telas, imagens, blur, sombras ou animações novas. Uma cena é
descartada antes de criar a seguinte. Cada cena nova é construída em uma
passagem limitada, somente quando solicitada; o fallback conserva seu stage.
Não há ganho de heap ou tempo declarado sem medição física.

## Validação de software — 2026-09-28

`idf.py clean build` passou para ESP32-P4 com ESP-IDF 5.5.4. Imagem
`0x2b37c0` bytes, 66% livres no menor slot de 8 MiB; tabela A/B preservada.
Logs de build sem warnings de compilação. O sandbox inicialmente bloqueou
a execução do Python do IDF; o build com execução autorizada concluiu.

Descritor `45a4189-dirty`; SHA-256 da imagem `firmware/build/np2_p4.bin`:
`9B3ADAFBD28DC442E0878825298EFB48D447E4719ACB0ED065482981C021F0B6`.
O mapa mostra `s_ui` com 3528 bytes, aumento estático de 208 bytes sobre a
base de 3320. Isto não mede o heap das árvores LVGL.

Configuração efetiva: RGB565, três FB, TRIPLE_PARTIAL, TWDT 5 s, reset C6
ativo baixo e auto-suspend desligado. C6 não foi alterado. Nenhum novo
resultado de flash, boot, render, WDT ou persistência física foi capturado.

## Aceite por flash manual

1. Home: abrir o drawer, entrar em Preferências e voltar para Home. Conferir
   que clima/mercado/relógio continuam como antes; engrenagem abre Perfil.
2. Preferências: abrir Perfil, conferir saudação, card Conta, avisos de edição,
   Tela inicial e Abrir preferências. Nenhum controle de saudação deve existir.
3. Tocar as cinco linhas do hub, abrir os controles/modais correspondentes
   no fallback e voltar pelo botão Preferências. Testar também o drawer Home.
4. Wi-Fi no fallback: sair com senha/teclado ou confirmação abertos. Reabrir
   e conferir que a sessão anterior foi cancelada e o teclado continua operável.
5. Repetir 100 ciclos Home/Preferências/Perfil/Settings, incluindo toques rápidos
   e saída antes da revelação do card. Registrar WDT/panic, heap interno/PSRAM,
   maior bloco, margem de pilha e latência de toque/render.

Registrar commit, hash P4, versão/hash C6 instalado, unidade/BOM, configuração,
data e logs sanitizados. Sem flash ou evidência física novos nesta fase.

## Ajuste de entrada — 2026-09-28

Por solicitação do operador após o primeiro flash, a engrenagem do header
abre Perfil em Home, Preferências e Settings anterior. Em Perfil ela mantém
a mesma cena. Abrir preferências continua levando ao hub; o drawer mantém
seu acesso direto ao hub. Diagnóstico deixa de ser o destino da engrenagem.

## Preferências conforme referência — 2026-09-28

Painel único em 1024x600, título com engrenagem em círculo azul e subtítulo.
Cinco linhas uniformes, sem rolagem, com blocos de ícones de fonte, títulos,
descrições e chevrons. Tela/Sistema usam cor neutra, Wi-Fi/Fuso azul e
Notificações vermelho. Toda a linha é o alvo de toque; o bloco decorativo
do ícone não intercepta o evento. A linha apresenta feedback ao pressionar.

Glyph desktop_windows U+E30C adicionado ao subset Material de 24 px;
schedule U+E8B5 já estava disponível. A engrenagem U+E8B8 foi adicionada
ao subset de 48 px. Conversor local `lv_font_conv@1.5.3`, fonte Material
Symbols Rounded existente. Comparação dos codepoints confirma que nenhum
glyph anterior foi removido; métricas de altura/baseline foram preservadas.

Navegação/fallback continuam iguais. No flash manual, conferir ícones,
centralização, textos sem cortes, última linha dentro do painel e toque
também sobre o bloco do ícone. Build não é evidência de aceite visual.

`idf.py build` final passou em P4/IDF 5.5.4, imagem `0x2acc30` bytes,
67% livres no slot de 8 MiB, sem warnings de compilação no log final.
Descritor `9159402-dirty`; SHA-256:
`A01527F871ED3E8E56E18CBD1D1967C386E7533913EB00F3D228975CDF9A3BCE`.
Nenhum flash foi executado; WDT, schema offline e pipeline de display
permanecem sem alterações.

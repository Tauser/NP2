# Spotify — ensaio de pareamento no P4

23/10/2026, ADR-106. Laboratório, sem reprodução habilitada. O sinal PCM
aprovado em 22/10 é evidência separada em SPOTIFY-AUDIO-PIPELINE.md.

## Comportamento

`NP2_SPOTIFY_PAIRING_BENCH=ON` adiciona **Preferências → Sistema → Parear
Spotify**. O botão libera manutenção USB por 60 s; chaves não entram na UI.
O helper lê `credentials/spotify-client.json` (ignorado) e transmite uma
solicitação privada. Uma tentativa por boot; app/conta não são persistidos.

O worker valida o app pelo executor HTTPS único e autentica a assinatura
RSA/SHA-1 do AP antes de enviar conta. Então anuncia **NovaPanel teste XXXX**
em `_spotify-connect._tcp`, porta 8080, `/spotify_info`, durante até 180 s.
Máximo: seis POSTs, corpo 4 KiB, cabeçalhos 1 KiB, URI 128 B e dois sockets.
Sonoff LAN continua dono de mDNS; o ensaio não inicia/libera esse daemon.

Após o blob, fechar HTTPD/anúncio e testar a conta em nova sessão com AP
autenticado. Limpar app, blob, username e token reutilizável ao terminar.
Não iniciar Mercury, TrackPlayer, decoder ou PCM. Fases filtradas: 1 início;
2 app validado; 3 servidor autenticado; 4 descoberta; 5 blob recebido;
6 conta autenticada; 7 expiração; 8 falha. Fase 6 não significa áudio tocando.
Nenhum corpo, URL, chave ou identidade de conta entra no log.

## Fronteiras e recipe

- AP/Shannon até 8 KiB; mailbox até dois pacotes; Mercury até 16 partes,
  sem fragmentação. Recusar MAC incorreto, truncamento e tamanhos incoerentes.
- Âncora pública AP id 0, PKCS#1 v1.5/SHA-1 sobre a chave DH, antes de login.
  Referência: [librespot handshake](https://github.com/librespot-org/librespot/blob/dev/core/src/connection/handshake.rs).
- Checar RNG, DH, digest/HMAC e PBKDF2. DH remoto de 96 B, `1 < chave < primo-1`.
  Base64/blob limitados e HMAC obrigatório. Nanopb recusa erros/campos acima
  do destino; wrappers Vorbis não propagam exceções por C e validam formato.
- TLS/HTTP/JSON internos; liberar workspace de configuração antes de criar
  worker em outro núcleo. Pilhas PSRAM explícitas: worker 12 KiB, HTTPD 8 KiB.
- Exceções C++ e pool emergencial 2 KiB só nesse perfil. CMake recusa ON com
  Kconfig desligado: `-fexceptions` sozinho não habilita a libstdc++ do IDF.
- HTTP mantém um job/conexão. Contenção espera sem outro job; ocupado permite
  retry por até 20 s desde a entrada. Offline falha imediatamente. Guarda de
  conclusão tardia de 5 s, sem renovar prazo por tentativa.

`native_libraries.cmake` é comum ao harness e produto; `prepare_compat.py`
gera cópias, sem editar checkouts. `source-lock.json` fixa os quatro pins,
verificados por `verify_sources.py`. Adaptações de player/buffers são apenas
candidatas: o linker não inclui TrackPlayer/Vorbis nessa imagem.

## Reproduzir

```powershell
tools/run_spotify_packet_host_test.ps1
tools/run_spotify_pairing_host_test.ps1
tools/run_spotify_http_host_test.ps1
tools/run_spotify_bridge_host_test.ps1
tools/run_spotify_cdn_host_test.ps1
C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe tools/spotify_pairing_helper_host_test.py
tools/run_settings_system_ui_host_test.ps1
tools/run_settings_system_ui_host_test.ps1 -SpotifyPairingBench
tools/run_spotify_pairing_bench.ps1
```

Build separado com configuração efetiva da imagem PCM normal e exceções
habilitadas; fragmento em `sdkconfig.pairing.defaults`. `-Flash -Port COM8`
grava só app P4; conferir USB antes. Nunca gravar o app vazio do harness.
Depois do boot, preparar helper e liberar USB fisicamente:

```powershell
C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe tools/pair_spotify_panel.py --port COM8 --output firmware/build/spotify-pairing-20261023/pairing-sanitized.log
```

Só na fase 4, selecionar **NovaPanel teste XXXX** no Spotify do celular na
mesma rede. O botão não altera autorização Google. Após tentativa aceita
que expire/falhe, reiniciar P4 antes de outro ensaio.

## Evidência disponível

Passaram testes host de pacotes (20 mil mutações), JSON/form privado,
helper (3 grupos), HTTP/parser/codec, refs/prazos da ponte, CDN, buffers de
rede e UI normal/laboratório (100 ciclos). CDN usa cifra identidade: não
prova AES/Vorbis. Primeiro build completo encontrou dois erros de indentação
no novo C++, corrigidos. Warnings externos não significam código qualificado.

Build completo e app-flash em `firmware/build/spotify-pairing-20261023`.
Capture de boot após reset, 75 s, sem fault: P4 rev103, PSRAM 32 MiB, SD com
nove fotos, preferências restauradas. Google token/Tasks/Calendar 200;
resultado 0, integridade 1, mínimo interno 111.207 B, após 165.395 B,
maior bloco 96.256 B, stack rede 4.148 B, app_loop 2.232 B. Clima, BCB,
Alternative.me e três brapi 200. Sem prova CoinGecko HTTP nesse intervalo.

Estáticos: SRAM 70.792 B, BSS externa 136.336 B; placement passou. ELF contém
LoginBlob/verificação AP, sem TrackPlayer, `ov_read` ou probe sintético.
Hashes efetivos em `artifact-hashes.json` e BRINGUP-EVIDENCE.md. C6 continua
3.0.6, sem gravação; seu hash instalado não foi recapturado.

Fases 2–4 observadas após liberação física: app validado em 0,6 s, AP com
assinatura validada em 1,8 s e anúncio mDNS iniciado. Ambas as solicitações
do executor retornaram DNS/NTP/HTTPS ESP_OK. Heap interno por fase:
166.467 → 166.135 → 164.195 → 160.147 B; maior bloco 96.256 B, stack mínima
do worker 9.244 B, integridade 1. São amostras por fase, não o mínimo de heap
durante TLS/DH. Após seleção no celular, blob recebido em 164,8 s (fase 5):
interno 165.923 B. Conta autenticada em 166,0 s (fase 6): interno 162.907 B.
Nas duas fases, maior bloco 96.256 B, stack mínima 9.244 B e integridade 1.
O operador confirmou que o receiver apareceu e foi selecionado. Esse ensaio
termina após autenticação: não iniciou música nem decoder.
Faltam memória/stack do HTTPD e recuperação após falha. Reprodução, throughput CDN,
decoder e concorrência com alarmes/UI/rede continuam pendentes. Não fecha
integração Spotify, ADR-103 ou G3/G6.

Primeira espera USB de 300 s encerrou sem aceite/liberação física observada;
nenhuma fase Spotify foi iniciada. Helper fechou COM8 e limpou seu payload.
Após confirmação do toque, o helper foi retomado e as fases acima passaram;
não foi necessário regravar nem reautorizar Google.

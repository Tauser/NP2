# Entrada PCM para o futuro player Spotify

Estado em 22/10/2026: caminho PCM preparado e sinal sintético ouvido pelo
responsável na placa. Não há pareamento Connect, decoder Vorbis ativo,
faixa Spotify ou UI de player instalada. Esta etapa complementa ADR-105 e
SPOTIFY-NATIVE-INVESTIGATION.md; não encerra os gates de streaming.

Incremento de 23/10: laboratório de pareamento vinculado ao firmware completo,
sem iniciar decoder/PCM. Estado atual e evidência em
[SPOTIFY-PAIRING-VALIDATION.md](SPOTIFY-PAIRING-VALIDATION.md).

## Dono e limites

- device_control_service continua sendo o único dono do ES8311/I2S, PCM16
  mono a 22.050 Hz. Nenhuma mudança de pinos, codec, taxa ou driver.
- music_pcm converte PCM16 LE estéreo/44.100 Hz para mono/22.050 Hz: downmix,
  FIR Q15 de 127 taps, decimação por dois, estado sem alocação por bloco.
  Coeficientes reproduzíveis por tools/generate_music_fir.py --check.
- Fila de 131.072 amostras (262.144 B, cerca de 5,94 s), explicitamente em
  PSRAM, sem fallback interno. Somente amostras decodificadas entram nela;
  JSON/TLS/corpos HTTP permanecem internos. Alocação e liberação no owner,
  apenas quando há uma sessão. Sem sessão, não alocar a fila no boot.
- Cada cópia/leitura tem máximo 256 amostras. Fila cheia devolve consumo zero
  para backpressure; não sobrescreve áudio antigo. Geração invalida o produtor
  anterior em begin/stop/flush; pausa retém o buffer e ganho é volátil.
- Alarmes e notificações existentes conservam prioridade; o owner só lê
  música quando o sintetizador estiver livre. Underrun breve escreve silêncio
  para manter pacing do codec. Nenhum produtor toca I2S ou persiste volume.

Não há AppState/UI novo nesta etapa. A futura integração deverá projetar
somente status público pelo app_loop, jamais IDs/tokens/blobs do Spotify.

## Testes host

tools/run_music_audio_host_test.ps1 passou com -Wall/-Wextra/-Werror:
fragmentos de um frame ou sete frames e capacidade reduzida produzem saída
idêntica; downmix oposto cancela, DC em escala máxima não estoura; fila
limita cópias, wrap, ganho/pausa, geração e backpressure. Falha de alocação
não usa SRAM; stop/push concorrentes e liberação pelo owner passaram.

Resposta de amplitude relativa medida em sinais sintéticos de um segundo:

| Entrada | Saída relativa |
|---|---:|
| 1 kHz | 0,999668 |
| 8 kHz | 0,999534 |
| 12 kHz | 0,001193 |
| 16 kHz | 0,000220 |
| 20 kHz | 0,000476 |

run_sound_melody_host_test.ps1 também passou: nove melodias, persistência
legada, prioridade do alarme sobre música, pausa, silêncio de underrun e
falha de codec. Host não comprova qualidade sonora, consumo de CPU ou
concorrência com TLS/SD/flash na placa.

## Ensaio físico limitado

NP2_MUSIC_AUDIO_BENCH é OFF por padrão. Quando ON, inclui music_audio_probe.c
e executa uma única prova após 45 s na task de manutenção já existente.
Workspace interno limitado, no máximo dez segundos, sinal triangular baixo
de 1 kHz/3 s, gerado em estéreo/44.100 Hz, convertido e enviado à fila real.
Não usa credenciais, rede ou arquivos; não inicia outro dono de áudio.

Build limpo do P4 em firmware/build/music-owner-bench-20261022, IDF5.5.4,
target explícito; 2.210 etapas. app-flash COM8/460800, aplicação0x20000,
hash verificado. Placa Waveshare P4v1.3/rev103, NOR/PSRAM32MiB, C6 reportado
3.0.6, sem alteração de C6 (SHA instalado não obtido). Base Git777abde com
alterações locais preservadas. sdkconfig/partições/dependências preservados.
Logs sanitizados no diretório ignorado do build.

Resultado: probe0, 66.150 amostras mono, pico de fila50.944 amostras,
buffer262.144B e queda observada de PSRAM262.148B (inclui overhead).
PSRAM antes/depois14.024.544B, buffer liberado integralmente. Memória
interna mínima **observada por amostragem**172.879B; não é um monitor global
de pico. Stack livre da task de manutenção3.432B, integridade1. Responsável
confirmou: “Ouvi normalmente”. Captura120s sem marcador de panic/assert/watchdog.

Nessa captura, Google terminou antes do sinal: HTTP200/resultado0, mínimo
interno120.399B, rede livre4.148B, app_loop2.232B, integridade1. Não declarar
TLS e música concorrentes ensaiados. Fotos e melodias inicializaram; HTTP200
Open-Meteo/BCB/Alternative.me/brapi. Não é ensaio de CoinGecko autenticado.

Após a prova, reconfigurar explicitamente NP2_MUSIC_AUDIO_BENCH=OFF e gravar
a imagem normal. A remoção do autoteste e os hashes finais são registrados
ao concluir a captura normal. Não deixar a imagem de laboratório como versão
normal por conveniência.

### Imagem normal reinstalada

NP2_MUSIC_AUDIO_BENCH=OFF confirmado em CMakeCache. ELF normal não contém
music_audio_run_probe; ELF da prova contém o símbolo (checado com pyelftools).
app-flash final COM8/460800, hash verificado, aplicação0x20000. Captura100s
sem marcador de fault e sem execução do autoteste; COM8 liberada.
Imagem normal3.543.760B, 58% livres na partição de8MiB, dados estáticos
internos64.032B/BSS externa136.336B; checker de layout passou. Build limpo
da prova e incremental normal sem warnings registrados.

| Artefato | SHA256 |
|---|---|
| Bin normal instalado | `642C9BF00FEF2C051B0B965A44E88B2774C144F3E6919D507456101A1AFCD35E` |
| ELF normal | `A2BF5DC548CDDB686193B7D74394A6E82EB97AF83CF05B54DBB2B265DDAE9ADA` |
| Bin da prova | `6A49616007BBB33126FF4D75AAF67AA4BD0A54E869F2C8C85190A5B821A40546` |
| ELF da prova | `AA4EA12D74670A48D1CD78563BFCC9C6A57851F2A9E87BED4976B8785CEF4BB6` |
| sdkconfig | `8CC2CF3B76F9E1B51BF63AD2F40E0BAF8FBB7D62703F638EF95B2BC4675663CA` |
| Partições bin | `66388633FBA5299ADD799FB294C2167B9C6FB690E0CC2437571176525633BB23` |

Captura normal: fotos/melodias presentes; Google token/Tasks/Calendar200,
sync0, mínimo interno120.367B, depois174.607B, maior bloco102.400B, rede
livre4.208B, app_loop2.228B, integridade1. Open-Meteo/BCB/Alternative.me e
brapi200. Não atribuir nova chamada CoinGecko ao ciclo BTC observado.
Manifesto público de310 arquivos e artifact-hashes.json no diretório do
build; sem credenciais/managed_components. C6 e partições não regravados.

## Pendências do player

Ligar o decoder à conversão/fila com reset por faixa/seek, validar mudança
de canais/taxa no meio do stream e backpressure nos callbacks. Medir heap e
stack do decoder ativo, tratar falhas/stop sem exceção cruzando callbacks C.
Verificar músicas longas, pause/resume, avanço/seek, alarmes, reconexão,
throughput do CDN e coexistência com agenda/OTA/SD/flash. A ponte HTTP atual
fecha TLS por request; compilação e este sinal não provam vazão de streaming.

# Ensaio de compilação Spotify nativo

Esta ferramenta verifica as bibliotecas de um candidato experimental.
Não é um player, não se conecta ao Spotify e não deve ser gravada na placa.
O firmware normal não depende deste projeto. A variante opt-in de pareamento
reutiliza somente a recipe de bibliotecas, nunca o app vazio. Evidência e gates em
[SPOTIFY-NATIVE-INVESTIGATION.md](../../docs/SPOTIFY-NATIVE-INVESTIGATION.md).

## Preparação

Fontes locais em `firmware/build/spotify-investigation-20261022/cspot`.
O clone e todos os submódulos precisam coincidir com `source-lock.json`.
Para obter a revisão fixada a partir da rede:

```powershell
git init firmware/build/spotify-investigation-20261022/cspot
git -C firmware/build/spotify-investigation-20261022/cspot remote add origin https://github.com/philippe44/cspot.git
git -C firmware/build/spotify-investigation-20261022/cspot fetch --depth 1 origin 99e5c95ad6d862a1b7845dda81fd9c8233b45b9f
git -C firmware/build/spotify-investigation-20261022/cspot checkout --detach FETCH_HEAD
git -C firmware/build/spotify-investigation-20261022/cspot submodule update --init --recursive --depth 1
```

Em uma instalação nova, criar um venv **Python 3.11** no diretório ignorado
`firmware/build/spotify-investigation-20261022/proto-env` e instalar nele:

```powershell
firmware/build/spotify-investigation-20261022/proto-env/Scripts/python.exe -m pip install -r tools/spotify_native_probe/generator-requirements.txt
tools/run_spotify_native_probe.ps1
```

O runner usa ESP-IDF 5.5.4 e a toolchain P4 já instalada. CMake 4 recebe a
política mínima 3.5 para os projetos externos antigos. Protobuf usa somente
o Python isolado; o Python do IDF não é modificado. O componente mDNS 1.5.3
vem do conjunto já resolvido em `firmware/managed_components`.

O alvo construído é `cspot`, junto de Bell e dependências, não o app.
O `app_main` vazio apenas satisfaz a descoberta do projeto ESP-IDF.
Os defaults aqui pertencem ao ensaio. Nunca copiá-los para o produto.

## Limites

Remoção de assembly Xtensa, alias de prioridade pthread, seleção de codecs, cstring/assinatura CivetWeb e
exceções nos alvos externos são adaptações locais do harness. Código de rede, parser,
protocolo e segurança do candidato não ficam aprovados por compilar.
Sem execução na placa: nenhum dado novo de RAM, áudio, login ou recovery.
Antes de incorporação, ler a licença GPL-3.0-or-later do candidato e
resolver os gates do documento de investigação.


O ensaio final limpo foi executado com:

```powershell
tools/run_spotify_native_probe.ps1 -BuildDirectory D:/Projetos/NP2/firmware/build/spotify-investigation-20261022/clean-build
```

## Adaptações limitadas de transporte e leitura

O ensaio atual substitui HTTPClient por uma interface C privada, desabilita
TLSSocket direto e valida respostas sem DOM JSON ilimitado. O gerador
prepare_compat.py cria cópias verificadas de LoginBlob/TrackQueue/CDNAudioFile/TrackPlayer
no build, preservando o checkout. workspace JSON fica em SRAM interna; logs
de URL de CDN são removidos. O binder não está conectado a um player no produto.

```powershell
tools/run_spotify_http_host_test.ps1
tools/run_spotify_bridge_host_test.ps1
tools/run_spotify_cdn_host_test.ps1
tools/run_spotify_native_probe.ps1 -BuildDirectory D:/Projetos/NP2/firmware/build/spotify-investigation-20261022/adapter-build
```

O teste CDN usa cifra identidade para verificar apenas fluxo/limites, sem
provar AES, Vorbis ou áudio. O firmware do painel recebeu a ponte inativa
e o limite de 8 KiB de cabeçalhos para regressão de rede; essa gravação é
distinta deste app vazio e não vincula as bibliotecas cspot/Bell. Pareamento,
throughput, memória do decoder, áudio e arbitragem continuam pendentes.

## Incremento de pareamento em 23/10

`native_libraries.cmake` agora é compartilhado com o laboratório integrado
ao firmware completo. `verify_sources.py` valida pins/checkouts. Cópias
limitadas incluem AP/assinatura RSA, Shannon/Mercury, nanopb e primitivas
criptográficas com retorno checado, além das adaptações anteriores.
O build completo usa exceções habilitadas no Kconfig e pool emergencial;
o harness de compilação não constitui prova de runtime.

`tools/run_spotify_pairing_bench.ps1` compila essa imagem separada;
`-Flash` grava apenas a aplicação P4. Após o botão físico de liberação USB,
`tools/pair_spotify_panel.py` lê o app local e registra somente fases fixas.
Não inicia player/decoder. Procedimento e limites em
[SPOTIFY-PAIRING-VALIDATION.md](../../docs/SPOTIFY-PAIRING-VALIDATION.md).

A opção explícita `-Playback` do runner compila o player, guard Vorbis e sink
PCM. `prepare_runtime.py` aplica limites e sincronização nas cópias privadas;
`prepare_vorbis.py` redireciona apenas as alocações do Tremor privado.
Testes próprios: `run_spotify_vorbis_host_test.ps1` e
`run_spotify_pcm_sink_host_test.ps1`. A variante sem `-Playback` continua
sem player. Procedimento e estado físico em
[SPOTIFY-PLAYBACK-VALIDATION.md](../../docs/SPOTIFY-PLAYBACK-VALIDATION.md).

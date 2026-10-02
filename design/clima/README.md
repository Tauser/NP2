# Ícones animados de clima

Os 20 SVGs nesta pasta são os arquivos-fonte selecionados para os dez estados
de `weather_condition` em dia e noite. Eles vieram de [Meteocons](https://meteocons.com/icons/),
de Bas Milius, distribuído sob [licença MIT](LICENSE-METEOCONS.txt).
Preserve essa atribuição ao distribuir os SVGs ou os quadros derivados.

O firmware não interpreta SVG. `tools/build_weather_icons.py` usa Chrome ou
Edge local para amostrar os ciclos SMIL a 8 fps, 160 × 160 px com transparência,
sem trabalho manual por quadro. Cada ícone gera **um** arquivo `NPWI` v1 com
24 bytes de cabeçalho e 24 ou 48 quadros BGRA8888, protegidos por CRC32.
Os 20 pacotes somam cerca de 73 MiB no microSD; o ícone atual e o anterior
usam dois slots de PSRAM. A UI usa `lv_animimg` e não faz leitura de arquivo.
O firmware aceita temporariamente os pacotes antigos de 96 × 96 px, mas só os
novos têm nitidez nativa na área de 160 × 160 px da Home.

```powershell
& 'C:\Users\Tauser\.platformio\penv\Scripts\python.exe' tools/build_weather_icons.py
.\tools\copy_weather_icons_to_sd.ps1 -DestinationRoot E:\
```

Substitua `E:\` pela raiz real do cartão. O script copia e verifica todos os
pacotes em uma pasta de staging, troca a pasta `np2/weather/icons/` e preserva
os pacotes anteriores em `np2/weather/icons-96-backup/`. Os backgrounds
existentes continuam em `np2/weather/`, embora a Home não os use mais.
Os `.bin` em `generated-160/` são derivados e não entram no Git. Se cartão ou
pacote faltar, a Home mostra o ícone estático da fonte do produto.

## Ícones estáticos da tela Clima

`static/` contém o pacote oficial `@meteocons/svg-static` versão 0.1.0 e sua
licença MIT. O firmware usa somente os SVGs em `static/fill/` selecionados pelo
gerador `tools/build_meteocons_static.py`: condições diurnas/noturnas, nascer e
pôr do sol, vento, umidade, índice UV, chuva e ícone indisponível. O gerador
pré-renderiza os SVGs em 32 × 32 e 48 × 48 pixels e grava os descritores LVGL
em `firmware/main/ui/assets/np_weather_static_icons.c`. O firmware não interpreta
SVG em execução. O ícone principal continua usando os pacotes NPWI animados da
microSD.

Para regenerar os descritores, instale o renderizador temporário e execute:

```powershell
$renderer = Join-Path $env:TEMP 'np2-meteocons-renderer'
New-Item -ItemType Directory -Force $renderer | Out-Null
pnpm --dir $renderer add @resvg/resvg-js@2.6.2
python tools/build_meteocons_static.py
```

O gerador requer também Pillow no Python. A dependência Node é usada somente no
host durante a geração; não entra no firmware. Preserve `static/LICENSE` ao
redistribuir os SVGs ou as imagens derivadas.

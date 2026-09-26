# Assets meteorológicos da Home

A Home atual usa o card sólido `#111820` e o ícone animado em
`/sdcard/np2/weather/icons/`. Os backgrounds descritos abaixo são legados e
não são mais carregados pelo firmware. Os ícones funcionam sem eles.

Para gerar os 20 pacotes animados a partir dos SVGs em `design/clima/`, execute
`tools/build_weather_icons.py` (saída em `design/clima/generated-160/`); para
copiá-los ao cartão, use
`tools/copy_weather_icons_to_sd.ps1 -DestinationRoot F:\` (substitua a letra
pela unidade atual). Ejete o cartão com segurança antes de recolocá-lo na
placa desligada.

## Backgrounds legados

Cada asset deve ser exportado em **RGB565 bruto**,
sem cabeçalho, com **560 x 376 px** (421120 bytes). Esse é exatamente o tamanho
externo do `weather_card`; o card não possui padding implícito e a imagem não é
redimensionada em tempo de execução.

O arquivo no cartão permanece compacto, com 1120 bytes por linha. Ao carregá-lo,
o firmware o posiciona em um slot PSRAM com stride de 1152 bytes exigido pelo
renderer LVGL desta placa. São somente 32 bytes de padding limpos por linha:
não há escala, decodificação nem conversão de cor durante o desenho.

Mantenha os 20 arquivos-fonte em `firmware/main/ui/assets/weather/` e copie-os
para o cartão microSD em `np2/weather/` com
`tools/copy_weather_assets_to_sd.ps1`. O firmware monta o cartão em `/sdcard`
e os lê de `/sdcard/np2/weather/`; eles **não** são embutidos na aplicação ou
na partição flash interna.

Os arquivos antigos que ainda estiverem nessa pasta com 462000 bytes são do
layout 500 x 462 e não devem ser copiados para a Home atual. O instalador
recusa esses arquivos antes de escrever no cartão.

```
np_bg_day_clear.bin                 np_bg_night_clear.bin
np_bg_day_partly_cloudy.bin         np_bg_night_partly_cloudy.bin
np_bg_day_fog.bin                   np_bg_night_fog.bin
np_bg_day_drizzle.bin               np_bg_night_drizzle.bin
np_bg_day_rain.bin                  np_bg_night_rain.bin
np_bg_day_snow.bin                  np_bg_night_snow.bin
np_bg_day_rain_showers.bin          np_bg_night_rain_showers.bin
np_bg_day_snow_showers.bin          np_bg_night_snow_showers.bin
np_bg_day_thunderstorm.bin          np_bg_night_thunderstorm.bin
np_bg_day_variable.bin              np_bg_night_variable.bin
```

Sem cartão, sem os arquivos ou com um arquivo de tamanho inválido, a Home usa
o card escuro neutro. Não há imagem de substituição, escala ou conversão em
runtime.

# Material Symbols Rounded no EEZ Studio

Use o arquivo TTF original `MaterialSymbolsRounded-Regular.ttf` que gerou as
fontes C. Nao selecione os arquivos `lv_font_material_*.c`: eles sao a saida
para o firmware, nao uma fonte que o EEZ consiga importar.

Configuracao comum:

- **Bits per pixel:** `4`
- **Use FreeType for rendering:** desligado
- **Symbols:** vazio

## Barra superior e icones pequenos

Crie tres fontes, uma para cada tamanho. As tres usam a mesma lista abaixo.

| Name | Font size (pixels) |
| --- | ---: |
| `np_material_16` | 16 |
| `np_material_18` | 18 |
| `np_material_24` | 24 |

Cole este texto no campo **Ranges** de cada uma:

```text
59475,57669,59701,61400,58131,58132,58133,58134,62039,62038,62037,62036,62035,62034,62033,62032,62031,59380,57769,57767,59498,59999,59573,60357,58826,58829,58689,59506,57987,61376,59527,59530,58356,59534,59832,57584,62435,59833,61330,61810,57387,57385,61494,58373,59152,57762,57796,58837,62591,59574,59576,61297,59418,58919,61763,57421,57422,57423,57424,57346,58045,58061,59416,63103,61795,61796,61797,61812,61814,62329,62330,60379,63007,63006,63005,58942,58570,58585,58952,61211,61210,61783,57868,58821,58823,58834
```

Os tres arquivos C anexados contem **85 icones**; a lista acima adiciona o
menu e, portanto, importara **86 icones**. Inclui, entre outros: menu
(`58834` / `U+E5D2`),
notificacoes (`59380` / `U+E7F4`), Wi-Fi (`58942` / `U+E63E`), Wi-Fi desligado
(`58952` / `U+E648`), sol (`59418` / `U+E81A`), nevoa (`59416` / `U+E818`) e
chuva (`61814` / `U+F176`).

## Icones grandes de clima

Crie uma quarta fonte:

| Name | Font size (pixels) |
| --- | ---: |
| `np_material_48` | 48 |

No campo **Ranges**, cole:

```text
58045,58061,59416,59418,60379,61783,61810,61814,63005-63007,63103
```

Ela tem **12 icones**. Os que precisamos imediatamente para o painel sao:

| Uso | Unicode | Decimal |
| --- | --- | ---: |
| Sol | `U+E81A` | 59418 |
| Nevoa | `U+E818` | 59416 |
| Trovoada | `U+EBDB` | 60379 |
| Ceu limpo | `U+F157` | 61783 |
| Parcialmente nublado | `U+F172` | 61810 |
| Chuva | `U+F176` | 61814 |
| Neve | `U+E2CD` | 58061 |

## Uso nas telas

Depois da importacao, adicione um `Label`, selecione a fonte Material correta e
insira o caracter pelo codigo Unicode. A barra da Home usa `U+E5D2` (menu),
`U+E63E` (Wi-Fi) e `U+E7F4` (notificacoes). O clima usa `U+E81A` em 48 px no
estado de ceu limpo.

O menu e o unico icone desta configuracao que nao esta nos tres arquivos C
existentes. Isso deixa a fonte importada no EEZ pronta para a barra do modelo;
ao gerar o codigo do EEZ, use a fonte gerada em vez das fontes C antigas para
evitar que a simulacao e o firmware tenham conjuntos diferentes.

Mantenha a fonte Material apenas nos labels de icones. Textos e numeros devem
continuar usando Montserrat para preservar acentos e legibilidade.

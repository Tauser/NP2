# Índice imutável de fusos

O catálogo continua com 462 escolhas e a mesma ordem persistida. Os índices
0–4 permanecem São Paulo, Brasília (alias da mesma IANA), Buenos Aires,
Nova York e Londres. CSV/POSIX, perfil de onboarding, schema e backend não
mudaram.

`timezone_catalog_index.inc` contém 457 offsets de 16 bits no CSV em flash,
evitando percorrer centenas de linhas para cada consulta da UI. Não há índice
montado em RAM nem alocação por consulta. `.gitattributes` fixa LF no CSV,
pois offsets dependem dos bytes. CMake rejeita índice desatualizado.

Metadados de país vêm dos snapshots públicos IANA em `tools/data`: `zone.tab`,
`iso3166.tab` e links extraídos de `tzdata.zi`, pacote Python tzdata 2026.2
(base 2026b; o arquivo local de origem identifica `2026b-dirty`). Os snapshots
são versionados e de domínio público; somente nomes/aliases foram extraídos,
sem substituir regras POSIX ou adicionar I/O. 427 escolhas têm associação
geográfica conhecida; zonas restantes não ganham país fictício. Nomes comuns
foram traduzidos para português; demais países usam nomes ISO em inglês.

`timezone_catalog_standard_offset` interpreta o offset padrão POSIX com sinal
geográfico UTC. Ele não calcula nem afirma o offset atual durante horário de
verão. A UI identifica essa informação como “Offset UTC padrão”.

Reproduzir/verificar:

```powershell
python tools/generate_timezone_index.py
python tools/generate_timezone_index.py --check
./tools/run_settings_timezone_ui_host_test.ps1
```

O teste compara todas as entradas não primárias com o CSV e verifica escolhas
legadas, países, offsets e limite. A geração não altera o CSV nem seus índices.

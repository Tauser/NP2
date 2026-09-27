# NovaPanel — referências visuais v5

Esta pasta contém somente material de referência para discutir e revisar a
experiência visual do painel. Ela não é compilada pelo firmware.

## Conteúdo

- `mockups/`: SVG das telas na resolução de 1024 × 600.
- `images/`: PNGs correspondentes aos mockups.
- `tools/gen_mockups.py`: gerador dos mockups.

Os arquivos C da UI pertencem ao firmware:

```text
firmware/main/ui/
  core/       tokens, estilos e componentes LVGL
  screens/    Boot, Home e telas futuras
  fonts/      fontes compiladas para o painel
```

Os mockups podem mostrar funcionalidades futuras que ainda não possuem serviço
ou dado real. Eles são referência de produto; não autorizam adicionar campos
simulados à UI ativa.

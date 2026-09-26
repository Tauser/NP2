# Projeto visual oficial LVGL

Este diretório é a fonte editável pelo **LVGL Editor oficial** para a UI do
NovaPanel. O projeto usa o esquema aceito pela extensão instalada e o alvo
físico de 1024 × 600 da Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.

## Abrir no VS Code

1. Abra `NovaPanel-UI-LVGL.code-workspace` no VS Code.
2. A extensão recomendada `LVGL.lvgl-editor` já está instalada.
3. Execute `LVGL: Open Editor` pela paleta de comandos.
4. Edite `screens/boot.xml` ou `screens/home.xml` no modo Design.

`globals.xml` concentra tokens visuais e os subjects que receberão dados de
`AppState`. Os valores iniciais representam o estado indisponível honesto; não
use textos de demonstração como se fossem dados do painel.

## Integração no firmware

O C em `../core` e `../screens` continua sendo a UI compilada nesta primeira
etapa. Após uma alteração visual aprovada, use **Generate code** no editor e
integre o C exportado sob `firmware/main/ui/generated/`. A ponte para
`AppState` deve ficar em arquivo não gerado, pois arquivos `*_gen.c` e
`*_gen.h` são substituídos em toda exportação.

Só depois de revisar o C exportado, atualizar o `CMakeLists.txt`, compilar e
validar na placa é que a tela exportada substitui a versão direta atual.

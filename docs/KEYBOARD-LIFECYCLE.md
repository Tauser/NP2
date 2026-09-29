# Teclado global — lifecycle (28/09/2026)

O teclado LVGL oficial permanece único, propriedade de product_ui_state_t, 1024 px no rodapé. DEFOCUSED agenda lv_async_call; a reconciliação procura campos registrados visíveis com foco e troca o target. Interação com teclado preserva a sessão. Hide/clear/destroy cancelam o async antes de invalidar targets. DELETE devolve o binding ao pool de oito. Bind verifica todos os registros antes de ocupar slot vazio; destroy remove callbacks dos campos sobreviventes.

Teste real LVGL host: A→B, retarget antes de callback de B, interação com tecla, ausência de foco, rebind após slot liberado, fechar/deletar com async pendente, destruir dono mantendo campo vivo, largura/ancoragem. Também passou a regressão Wi-Fi de 100 ciclos. idf.py -D IDF_TARGET=esp32p4 build passou. Testes de toque/foco e WDT físicos aguardam flash manual.

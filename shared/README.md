# Contratos compartilhados

`shared/` é a fonte de verdade dos payloads que atravessam firmware, testes e
integrações futuras. O schema v4 de dados offline usa números inteiros para
evitar ponto flutuante no estado e no cache do P4. Ele inclui vento, sensação
térmica, UV, máxima/mínima/volume do BTC e variação da PTAX entre as duas
últimas observações. O Ibovespa permanece indisponível nesta etapa. Os caches
v1 a v3 continuam legíveis e seus campos novos começam indisponíveis.

O contrato não contém credenciais, URLs, headers HTTP, corpos brutos nem JSON
arbitrário. A UI recebe somente a projeção validada.

Quando `available=true`, todos os valores do domínio e `observedAtUnixS > 0`
são obrigatórios. Quando indisponível, o modelo binário mantém esses campos
zerados e `stale=false`. `weatherVisual` só é disponível quando há clima
válido; ele não contém imagem, URL ou dado sensível. Veja os exemplos em
`examples/`.

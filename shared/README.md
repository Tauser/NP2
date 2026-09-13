# Contratos compartilhados

`shared/` é a fonte de verdade dos payloads que atravessam firmware, testes e
integrações futuras. O schema v1 de dados offline usa números inteiros para
evitar ponto flutuante no estado e no cache do P4.

O contrato não contém credenciais, URLs, headers HTTP, corpos brutos nem JSON
arbitrário. A UI recebe somente a projeção validada.

Quando `available=true`, todos os valores do domínio e `observedAtUnixS > 0`
são obrigatórios. Quando indisponível, o modelo binário mantém esses campos
zerados e `stale=false`. Veja os exemplos completo e parcial em `examples/`.

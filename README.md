# Sistema de Avaliação de Desempenho em Estruturas de Dados: Telemetria Automotiva

Trabalho prático da disciplina de Estruturas de Dados (Engenharia de Controle e Automação, UNESP Sorocaba), desenvolvido sob a orientação do Prof. Dr. Leopoldo André Dutra Lusquino Filho. O repositório traz uma implementação em C de um processador de fluxo contínuo de dados. O objetivo é realizar a análise e o benchmarking de diversas estruturas de dados, observando seu comportamento e desempenho frente a restrições de memória, carga de processamento e presença de anomalias nas entradas.

O sistema atua como o módulo de telemetria e datalogger de uma Unidade de Controle Eletrônico (ECU) programável, simulando a aquisição e o processamento de dados de um motor de alta performance (baseado em parâmetros de um AP 1.6 8v turbo). O fluxo contínuo emula a varredura de sensores operando em alta frequência e em tempo real, gerando registros de:

- Rotação do motor (RPM)
- Pressão do óleo e do turbocompressor
- Temperatura do líquido de arrefecimento
- Posição da borboleta (TPS) e velocidade

O *dataset* consistirá em um fluxo contínuo de pelo menos 10.000 amostras sintéticas, com a introdução de ruído gaussiano para simular anomalias de leitura, permitindo a execução de operações de inserção, remoção, busca, detecção de anomalias e filtragem de logs.
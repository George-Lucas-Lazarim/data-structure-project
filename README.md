# Sistema de Avaliação de Desempenho em Estruturas de Dados: Telemetria Automotiva

Trabalho prático da disciplina de Estruturas de Dados (Engenharia de Controle e Automação, UNESP Sorocaba), desenvolvido sob a orientação do Prof. Dr. Leopoldo André Dutra Lusquino Filho. O repositório traz uma implementação em C de um processador de fluxo contínuo de dados. O objetivo é realizar a análise e o benchmarking de diversas estruturas de dados, observando seu comportamento e desempenho frente a restrições de memória, carga de processamento e presença de anomalias nas entradas.

O sistema atua como o módulo de telemetria e registrador de dados de uma Unidade de Controle Eletrônico (ECU) programável, simulando a aquisição e o processamento de dados de um motor de alta performance (baseado em parâmetros de um AP 1.6 8v turbo). O fluxo contínuo emula a varredura de sensores operando em alta frequência e em tempo real, gerando registros de:

- Rotação do motor (RPM)
- Pressão do óleo e do turbocompressor
- Temperatura do líquido de arrefecimento
- Posição da borboleta (TPS)

O *dataset* consistirá em um fluxo contínuo de pelo menos 10.000 amostras sintéticas, com a introdução de ruído gaussiano para simular anomalias de leitura, permitindo a execução de operações de inserção, remoção, busca, detecção de anomalias e filtragem de logs.

## Arquitetura e Estrutura de Dados

A arquitetura do sistema utiliza cinco estruturas de dados para viabilizar o processamento em tempo real. A fim de evitar lentidão no sistema e o consumo excessivo de recursos, o fluxo de trabalho divide as responsabilidades em quatro frentes: Recepção de Dados em Alta Frequência, Tradução e Indexação Estática, Gerenciamento de Imprevistos e Armazenamento do Histórico.

### Recepção de Dados em Alta Frequência

Atua como o *buffer* de entrada. Os dados brutos dos sensores (RPM, TPS, pressões,...) chegam em alta frequência e são imediatamente enfileirados em uma Fila Circular Estática (estrutura otimizada).

Em vez de utilizar uma Fila Dinâmica com listas encadeadas, a Fila Circular foi alocada de forma estática e contígua na memória. O controle de entrada e saída é feito por aritmética de ponteiros. Isso elimina o uso contínuo das funções `malloc` e `free`, garantindo a recepção em tempo estritamente $O(1)$ e evitando a fragmentação da memória.

### Tradução e Indexação Estática

O laço principal retira o dado da Fila e verifica se ele ultrapassa os limites de segurança. Caso um parâmetro esteja fora do normal (como uma queda brusca na pressão de óleo), o código numérico do erro é passado para uma Tabela Hash.

A Tabela Hash atua como o dicionário de calibração, traduzindo o código bruto em um código de falha (DTC) e determinando a sua severidade instantaneamente em tempo estritamente $O(1)$.

### Gerenciamento de Imprevistos

Lida com o tratamento das anomalias detectadas pela etapa anterior utilizando duas estruturas em conjunto para evitar sobrecarga. Antes de acionar um alerta custoso, o identificador do erro passa por um Bloom Filter, um filtro probabilístico que verifica rapidamente se a mesma falha já foi reportada na janela de tempo atual. Se sim, o alerta é descartado.

Caso o erro seja diferente dos já reportados, ele entra em uma Heap Min-Max. Problemas graves (como perda de lubrificação) sobem imediatamente para o topo do vetor em tempo $O(\log n)$, forçando o sistema a priorizar ações de segurança rigorosas antes de processar alertas menores.

### Armazenamento Histórico

Paralelamente à análise de alertas, todos os pacotes de telemetria processados precisam ser armazenados para análise futura. Para isso, os dados são armazenados em uma Skip List, que guarda o histórico completo ordenado pelo tempo exato da leitura.

Como é uma estrutura probabilística baseada em saltos de ponteiros, a Skip List oferece a mesma eficiência de busca e inserção de uma árvore balanceada, operando em tempo $O(\log n)$. A grande vantagem é que ela elimina o altíssimo custo computacional das operações de rotação de nós em cenários de inserção massiva de dados.
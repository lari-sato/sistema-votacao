# Projeto Final de Computação Distribuída: Sistema de Votação Distribuído em Tempo Real

## Equipe  
Julia Santos Oliveira — RA: 10417672  
Larissa Yuri Sato — RA: 10418318  

---

## Visão Geral do Projeto

Este projeto implementa um **sistema distribuído de votação em tempo real**, baseado em **sockets TCP**, comunicação linha a linha e suporte a múltiplos clientes simultâneos.  
- O servidor controla toda a lógica da eleição (opções, votos, placar, encerramento). Deve lidar com concorrência, falhas simples e encerramento da eleição via comando administrativo.
- Os clientes se conectam, enviam comandos e recebem respostas seguindo o protocolo definido. Cada um deles vota uma única vez e pode solicitar o placar parcial, e ao final da eleição, o placar final. 

O sistema foi projetado para garantir **voto único por VOTER_ID**, concorrência segura com **threads e mutex**, além de registrar todos os eventos da eleição em um log persistente.

Durante a fase de testes, executamos um experimento com **10 clientes simultâneos**, validando comportamento concorrente, integridade dos votos e estabilidade da execução. 
- Os prints dos **resultados desse teste** estão documentados no **relatório**, e os arquivos gerados por ele (`eleicao_TESTE.log` e `resultado_final_TESTE.txt`) estão disponíveis no repositório.

---
## Como Compilar?

### Servidor:
- `gcc server.c -o server -pthread`
- `./server <porta> <opcoes>`

#### Por exemplo:
`./server 8000 T R A U E`

### Cliente:
  - `gcc client.c -o client`
  - `./client <host> <porta> <id>`

#### Por exemplo:
`./client 127.0.0.1 8000 TRAUE-TOP`

---

## Requisitos Solicitados

### Servidor deve:
- Aceitar mínimo 20 clientes simultâneos
- Manter lista configurável de opções (mínimo 3)
- Garantir voto único por `VOTER_ID`
- Responder com placar parcial
- Encerrar eleição por comando `ADMIN CLOSE`
- Registrar log (`eleicao.log`)
- Gerar `resultado_final.txt`

### Cliente deve:
- Informar `VOTER_ID`
- Ver opções
- Enviar 1 voto
- Consultar placar
- Encerrar conexão

---

## FIM !!!

# Instruções para Agentes de IA (AGENTS.md)

Este documento contém instruções estritas que **todos os Agentes de Inteligência Artificial** (incluindo assistentes de codificação, revisores, ferramentas de CI/CD automatizadas, etc.) devem seguir ao interagir com o repositório Sapphire.

## 1. Regras de Git e Commits
- **Commits Semânticos**: Utilize o padrão de *Conventional Commits* (ex: `feat:`, `fix:`, `chore:`, `docs:`, `ci:`, `test:`, `refactor:`).
- **Clareza**: As mensagens de commit devem ser claras, diretas e descrever exatamente o que foi feito e por quê.
- **Granularidade e Frequência**: Para **cada** mudança isolada ou etapa concluída, o agente **deve** realizar um commit. Não agrupe múltiplas mudanças não relacionadas no mesmo commit.
- **Verificação**: Antes de qualquer commit, garanta que o código compila corretamente (via CMake no MSYS2/MinGW ou ambiente equivalente) e que não existem erros de sintaxe.

## 2. Testes e Deploy
- **Testes Locais**: Antes de finalizar uma tarefa, o agente deve garantir que os testes existentes ainda passam ou rodar comandos de verificação solicitados pelo usuário.
- **Testes de Deploy**: Para mudanças relacionadas a CI/CD (`.github/workflows`), o agente deve considerar testar os arquivos YAML com ferramentas locais ou alertar o usuário sobre a necessidade de rodar os workflows remotamente para validar a configuração.
- **Artefatos**: Ao alterar processos de build, garanta que os binários (ex: `sapphire.exe`, `runner.exe`) continuem sendo empacotados corretamente.

## 3. Gestão de CHANGELOG
- Para **cada nova funcionalidade (feat)** ou **correção importante (fix)**, o arquivo `CHANGELOG.md` **deve ser atualizado imediatamente**.
- Adicione as entradas na seção `[Unreleased]` (Não Lançado) ou na versão alvo, documentando claramente as mudanças feitas.

## 4. Documentação e Exemplos por Versão
- Sempre que houver a preparação para o lançamento de uma **nova versão** do Sapphire (ex: de `v1.0.8` para `v1.0.9`), o agente **deve**:
  1. **Atualizar o `main.cpp`**: Alterar o arquivo `main.cpp` para refletir a nova versão exata e a data de lançamento atual.
  2. Criar um novo arquivo de exemplos de versão, no formato `.github/vX.Y.Z-examples.md` (ex: `v1.0.9-examples.md`), se houver novas funcionalidades para demonstrar.
  3. Documentar exemplos claros, com sintaxe apropriada, demonstrando como as novas funcionalidades ou correções podem ser utilizadas pelos usuários finais.
  4. Atualizar referências de documentação que apontam para a versão anterior.
  5. **Atualizar o Site Oficial**: O arquivo principal `index.html` (e/ou documentações públicas na pasta `site/`) deve obrigatoriamente ser atualizado para incluir os novos exemplos de código, atalhos de sintaxe e definições que foram recém-adicionados. A página do projeto sempre deve refletir as capacidades atuais da linguagem.

## 5. Manutenção de Código
- Preserve os comentários e docstrings que explicam o porquê de decisões técnicas.
- Siga estritamente o estilo de código já existente no repositório.
- Nunca adicione bibliotecas pesadas de terceiros ou dependências externas sem aprovação explícita do usuário.

## 6. Arquitetura e Bibliotecas Base
- **Bibliotecas Principais**: O Sapphire usa fortemente `SFML` (para gráficos, eventos e sistema) e `OpenSSL` (para criptografia/rede). Ao interagir com elas, mantenha a preferência por linkagem estática (`SFML_STATIC_LIBRARIES=ON`, `OPENSSL_USE_STATIC_LIBS=TRUE`).
- **Sem Dependências Inúteis**: Não introduza frameworks ou dependências pesadas (ex: Boost, Qt) para resolver problemas triviais. Mantenha a base de código enxuta e leve.
- **Isolamento Core x UI**: Mantenha a lógica central (interpretador, parser) separada da interface e gráficos.

## 7. Quirks e Particularidades de Build
- **Ambiente Windows**: O desenvolvimento e build oficiais no Windows utilizam **MSYS2 com MinGW-w64**. Ao criar scripts de CI ou instruções, sempre leve isso em consideração.
- **Caminhos de Diretório (Paths)**: Existe diferença entre barras do Windows (`\`) e Linux (`/`). Utilize `std::filesystem::path` para manipulação de rotas em vez de concatenar *strings* com barras manuais para garantir que a compilação Cross-Platform (Windows/Ubuntu) funcione de primeira.
- **Flags do CMake**: Não altere os diretórios hardcoded do MSYS2 (ex: `/mingw64/lib/cmake/SFML`) nos workflows ou arquivos principais sem avisar o usuário.

## 8. Boas Práticas e C++ Moderno
- **Gerenciamento de Memória**: Abandone `new` e `delete` explícitos. Use Smart Pointers (`std::unique_ptr`, `std::shared_ptr`) para prevenir *Memory Leaks* em construções novas.
- **Semântica de Move**: Faça o uso correto de `std::move` em passagens de objetos grandes, como vetores e mapas, para otimização de performance.
- **Constexpr vs Macros**: Prefira `constexpr` ou variáveis constantes em linha a `#define` (macros) para segurança de tipagem.
- **Warnings Zero**: O código não deve gerar novos avisos (*warnings*) de compilação. Se sua alteração causar um warning (ex: conversão não implícita de size_t para int), resolva imediatamente antes do commit.

> **Nota para a IA:** Ao iniciar qualquer tarefa neste repositório, você deve implicitamente aceitar e ler profundamente este documento e seguir todas as regras listadas.

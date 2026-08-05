# Sugestão de Padronização de Estrutura de Projetos (Sapphire, Plugins & Beryl)

Este documento apresenta uma proposta para padronizar e organizar a estrutura de projetos da linguagem **Sapphire**, de seus **Plugins** (gerenciados pelo **Topaz/Spark**) e do empacotador de executáveis **Beryl**. O objetivo é criar uma estrutura limpa, organizada e intuitiva (inspirada em boas práticas de ecossistemas modernos como Rust, C# e Node.js, mas simplificada e sem arquivos XML complexos), compartilhando nomes de diretórios e padrões de configuração.

---

## 1. Visão Geral da Padronização

Atualmente:
1. Um projeto Sapphire comum (`sapphire init`) cria arquivos diretamente na raiz do diretório e utiliza o arquivo `ProjectInfo.txt` (com sintaxe `Chave=Valor`).
2. O gerenciador de pacotes Topaz utiliza `PLUGIN.txt` (com sintaxe `chave: valor`), `sapphire.json` (JSON) e coloca arquivos sob subpastas como `src/`.
3. O empacotador Beryl utiliza o arquivo `BerylConfig.txt` (com sintaxe `Chave=Valor` e comentários com `//`).

Nossa proposta unifica essa experiência sob os seguintes pilares:
1. **Mesma Sintaxe de Manifestos:** Substituir a sintaxe mista (`=` e `:`) por um padrão único baseado em `chave: valor` (simples de ler por humanos e fácil de processar no código C++ usando o mesmo parser).
2. **Mesmo Estilo de Comentários:** Utilizar `#` para comentários em todos os arquivos de manifesto e configuração (`PROJECT.txt`, `PLUGIN.txt` e `BERYL.txt`).
3. **Nomes de Pastas Compartilhados:** Código-fonte sempre reside em `src/`, testes sempre residem em `tests/`, saídas de build sempre residem em `build/`, e recursos adicionais (imagens, fontes, etc.) residem em `assets/`.
4. **Propósitos Claros:** O desenvolvedor sabe exatamente onde colocar cada arquivo, mantendo a raiz do projeto limpa.

---

## 2. Estrutura Proposta para Projetos Sapphire Comuns

Um projeto executável comum do Sapphire terá a seguinte estrutura organizada:

```text
meu_app/
├── PROJECT.txt          # Arquivo de manifesto do projeto (sintaxe chave: valor)
├── DEPENDENCIES.txt     # (Opcional) Dependências do projeto gerenciadas pelo Topaz
├── src/                 # Todo o código-fonte do projeto (.sp)
│   ├── main.sp          # Arquivo de entrada principal
│   └── config/          # Subpasta opcional para configurações de script
│       └── theme.sp     # Definições de tema/estilo
├── assets/              # Recursos estáticos (imagens, áudio, fontes)
│   └── logo.png
├── tests/               # Testes unitários do projeto
│   └── main_test.sp
└── build/               # Destino de compilação (.sbc ou executável empacotado)
```

### O Manifesto `PROJECT.txt`
```yaml
# Manifesto do Projeto Sapphire
name: meu_app
author: Sapphire Developer
version: 1.0.0
entry: src/main.sp
output: build/app.exe
description: Um aplicativo incrível construído em Sapphire
```

---

## 3. Estrutura Proposta para Plugins Sapphire (Topaz)

A estrutura de plugins se mantém muito semelhante e alinhada à do projeto comum, garantindo compatibilidade e facilidade de aprendizado:

```text
meu_plugin/
├── PLUGIN.txt           # Arquivo de manifesto do plugin (sintaxe chave: valor)
├── DEPENDENCIES.txt     # Dependências do plugin
├── src/                 # Todo o código-fonte do plugin (.sp)
│   └── meu_plugin.sp    # Arquivo de entrada do módulo/biblioteca
├── tests/               # Testes unitários do plugin
│   └── plugin_test.sp
├── versions/            # Controle de versões internas utilizado pelo Topaz
│   └── v1.0.0/
└── build/               # Artefatos compilados, se aplicável (ex: C-API .dll/.so)
```

### O Manifesto `PLUGIN.txt`
```yaml
# Manifesto de Plugin do Topaz
name: meu_plugin
author: Plugin Creator
version: 1.0.0
entry: src/meu_plugin.sp
description: Um plugin utilitário para Sapphire
repository: github.com/user/meu_plugin
```

---

## 4. Estrutura Proposta para Configuração do Beryl

O Beryl, responsável por empacotar e compilar projetos em executáveis standalone nativos, deve seguir a mesma convenção.

```text
meu_app/
├── BERYL.txt            # Novo nome do arquivo de configuração do Beryl (chave: valor)
├── src/
│   └── main.sp
└── assets/              # Pasta de recursos que o Beryl irá embutir
```

### O Manifesto `BERYL.txt`
Substitui o antigo `BerylConfig.txt` e adota a padronização de sintaxe da suite Sapphire:

```yaml
# Configuração de Empacotamento Beryl
EntryFile: src/main.sp
OutputFile: build/meu_app.exe
Author: Sapphire Developer
Version: 1.0.0
IconPath: assets/app_icon.ico
AssetsFolder: assets
NoConsole: true
Compress: true
Encrypt: true
Optimize: true
RequireAdmin: false
SoftMode: false
```

*Nota:* O Beryl também deve ser capaz de ler as configurações de compilação diretamente do arquivo `PROJECT.txt` caso `BERYL.txt` não esteja presente, reduzindo a necessidade de múltiplos arquivos de configuração em projetos simples.

---

## 5. Tabela de Comparação e Alinhamento

| Recurso / Pasta | Projeto Sapphire Comum | Plugin Sapphire (Topaz) | Empacotador (Beryl) | Propósito / Padrão |
| :--- | :--- | :--- | :--- | :--- |
| **Manifesto** | `PROJECT.txt` | `PLUGIN.txt` | `BERYL.txt` | Define metadados com sintaxe comum `chave: valor` e comentários com `#`. |
| **Código-Fonte** | `src/` (ex: `src/main.sp`) | `src/` (ex: `src/lib.sp`) | `src/` (ex: `src/main.sp`) | Todo o código executável reside sob esta pasta. |
| **Dependências** | `DEPENDENCIES.txt` | `DEPENDENCIES.txt` | - | Lista de dependências baixadas/gerenciadas pelo Topaz. |
| **Pasta de Testes** | `tests/` | `tests/` | - | Armazena scripts de teste validados por `spark test`. |
| **Pasta de Saída** | `build/` | `build/` | `build/` | Guarda os binários, bytecodes e executáveis empacotados. |
| **Recursos Extras** | `assets/` | `assets/` | `assets/` | Pasta padrão para mídias, ícones, fontes e pastas embutidas. |

---

## 6. Benefícios desta Padronização

1. **Reutilização de Código no Interpretador (C++):**
   * O parser de metadados em [parser.hpp](file:///c:/Users/berna/Downloads/Sapphire/src/topaz/include/core/parser.hpp) pode ser reaproveitado e unificado com o parser do Beryl em [beryl.cpp](file:///c:/Users/berna/Downloads/Sapphire/src/beryl/beryl.cpp#L41) para ler `PROJECT.txt`, `PLUGIN.txt` e `BERYL.txt`.
   * Estruturas comuns de diretórios simplificam os comandos `init` e de empacotamento.
2. **Limpeza da Raiz do Projeto:**
   * Evita poluição de arquivos `.sp` soltos na pasta raiz do usuário.
3. **Curva de Aprendizado Mínima:**
   * Desenvolvedores usam exatamente o mesmo layout físico de arquivos e o mesmo formato de configuração em qualquer ferramenta do ecossistema Sapphire.

---

## 7. Próximos Passos Sugeridos para Implementação

Se você aprovar esta sugestão, podemos planejar as seguintes alterações no código C++:
1. **Atualizar `run_init` em [main.cpp](file:///c:/Users/berna/Downloads/Sapphire/src/main.cpp#L2553):** Modificar a criação de novos projetos para usar a pasta `src/`, criar `PROJECT.txt` com a sintaxe padrão e colocar os scripts dentro de `src/`.
2. **Ajustar caminhos de execução padrão no compilador:** Adaptar o resolvedor de caminhos do compilador e da máquina virtual para buscar por `src/main.sp` quando executado a partir da pasta raiz do projeto.
3. **Atualizar o parser do Beryl em [beryl.cpp](file:///c:/Users/berna/Downloads/Sapphire/src/beryl/beryl.cpp#L41):** Modificar o parser para ler o arquivo `BERYL.txt` no formato `chave: valor` e reconhecer comentários com `#`.
4. **Unificar os parsers em [parser.hpp](file:///c:/Users/berna/Downloads/Sapphire/src/topaz/include/core/parser.hpp):** Estender o parser para suportar e validar ambos os manifestos de maneira genérica.

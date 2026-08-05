# GEMINI_RULES.md — Padrões de Qualidade para Sapphire

> **Este documento define os padrões mínimos de qualidade para código Sapphire.
> Leia antes de qualquer implementação.**

---

## ⚠️ REGRA NÚMERO 1: NÃO ESCREVA CÓDIGO INCOMPLETO

**Stub functions são absolutamente proibidas neste projeto.**

Stub = função que retorna um valor fixo sem fazer nada real. **JAMAIS.**

### ❌ PROIBIDO (SERÁ REJEITADO):

```sapphire
// LIXO - Não faça isso
function det(A) {
    return 1.0;  // ← Stub
}

// LIXO - Não faça isso
function solve_quadratic(a, b, c) {
    return [0.0, 0.0];  // ← Stub
}

// LIXO - Não faça isso
function is_prime(n) {
    return true;  // ← Stub
}

// LIXO - Não faça isso
function interpolate(x0, y0, x1, y1, x) {
    return y0;  // ← Implementação parcial/incompleta
}
```

**Se você não pode implementar, não abra o arquivo.**

---

## ✅ PADRÕES OBRIGATÓRIOS

### 1. Cada Arquivo Deve Ter:

- ✅ Comentário de cabeçalho descritivo
- ✅ Constantes nomeadas no topo (não magic numbers)
- ✅ Múltiplas funções relacionadas (mínimo 5)
- ✅ Tratamento completo de casos especiais
- ✅ Comentários explicando algoritmos complexos
- ✅ Seções bem organizadas com demarcadores
- ✅ Função auxiliar no final (ex: `abs_MODULE`)
- ✅ Sem código comentado/desativado
- ✅ Sem TODO/FIXME/HACK sem implementação

### 2. Cada Função Deve Ter:

- ✅ Implementação completa (não stub)
- ✅ Tratamento de casos extremos
- ✅ Comentários para lógica não-óbvia
- ✅ Nomes descritivos de variáveis
- ✅ Sem valores mágicos sem explicação

### 3. Estrutura de Seção:

```sapphire
// ─────────────── NOME DA SEÇÃO ───────────────
// Descrição do que essa seção faz

function func1() {
    // implementação completa
}

function func2() {
    // implementação completa
}

// ─────────────── OUTRA SEÇÃO ───────────────
```

---

## 🚫 ERROS CAPITAIS (REJEIÇÃO AUTOMÁTICA)

```
❌ Stub functions (return valor_fixo)
❌ Algoritmos incompletos ou parciais
❌ Magic numbers sem constante nomeada
❌ Sem tratamento de divisão por zero
❌ Sem tratamento de índices fora do limite
❌ Sem tratamento de entrada inválida
❌ for loops (Sapphire não tem, use while)
❌ Código comentado/desativado no final
❌ TODO/FIXME que não foram feitos
❌ Função com 1-3 linhas (sem razão válida)
❌ Classe com só init() vazio
❌ Sem comentários de demarcação de seção
❌ Variáveis com nomes genéricos (x, y, z, temp, data)
❌ Sem separação de responsabilidades
```

**Qualquer um desses garante rejeição.**

---

## 📋 CHECKLIST POR ARQUIVO

Antes de enviar qualquer arquivo, responda:

### Estrutura e Organização

- [ ] Arquivo tem cabeçalho descritivo com `//` comentário?
- [ ] Constantes nomeadas estão no topo (PI, EPS, etc)?
- [ ] Código está dividido em seções com `// ─── NOME ───`?
- [ ] Cada seção tem 2+ funções relacionadas?
- [ ] Arquivo tem função auxiliar no final?

### Qualidade de Código

- [ ] **Nenhuma função é stub** (return valor_fixo)?
- [ ] Cada função faz algo real, testável?
- [ ] Todos os algoritmos são completos?
- [ ] Tratamento de casos especiais existe?
  - [ ] Divisão por zero?
  - [ ] Índices fora do limite?
  - [ ] Entrada inválida/nula?
  - [ ] Valores extremos (muito grandes/pequenos)?
- [ ] Variáveis têm nomes descritivos?
- [ ] Sem magic numbers em lugar nenhum?

### Legibilidade

- [ ] Funções complexas têm comentários explicativos?
- [ ] Algoritmos não-óbvios têm documentação?
- [ ] Nomes de funções descrevem o que fazem?
- [ ] Sem código comentado/desativado?
- [ ] Sem TODO/FIXME/HACK?

### Sem Proibições

- [ ] Sem `for` loops (use `while`)?
- [ ] Sem `// TODO`, `// FIXME`, `// HACK`?
- [ ] Sem funções vazias ou com `return 0.0`?
- [ ] Sem classes que são só containers vazios?

---

## 💡 EXEMPLO: BOM vs RUIM

### ❌ RUIM (Será Rejeitado)

```sapphire
// Matemática básica
var PI = 3.14;

function det(A) {
    return 1.0;  // ← STUB
}

function dot(u, v) {
    return 0.0;  // ← STUB
}

function norm(v) {
    var sum = 0.0;
    // TODO: implementar depois
    return 0.0;
}
```

**Problemas:**
- Todas as funções são stubs
- PI sem precisão
- TODO não feito
- Sem casos especiais

### ✅ BOM (Será Aceito)

```sapphire
// Operações vetoriais e matriciais fundamentais
// Implementação completa com tratamento de erros

var PI = 3.14159265358979323846;
var EPS = 1e-10;

// ─────────────── OPERAÇÕES VETORIAIS ───────────────

// Produto escalar (dot product)
// u · v = u1*v1 + u2*v2 + ... + un*vn
function dot(u, v) {
    if (len(u) != len(v)) {
        return 0.0;  // Vetores de tamanho diferente
    }
    
    var result = 0.0;
    var i = 0;
    while (i < len(u)) {
        result = result + u[i] * v[i];
        i = i + 1;
    }
    return result;
}

// Norma L2 (Euclidiana)
// ||v|| = sqrt(v1^2 + v2^2 + ... + vn^2)
function norm_L2(v) {
    if (len(v) == 0) return 0.0;
    
    var sum = 0.0;
    var i = 0;
    while (i < len(v)) {
        sum = sum + v[i] * v[i];
        i = i + 1;
    }
    return sqrt(sum);
}

// Normalizar vetor (retorna v / ||v||)
function normalize(v) {
    var n = norm_L2(v);
    if (n < EPS) {
        return v;  // Vetor nulo, não normaliza
    }
    
    var result = [0.0, 0.0, 0.0];
    var i = 0;
    while (i < len(v)) {
        result[i] = v[i] / n;
        i = i + 1;
    }
    return result;
}

// ─────────────── OPERAÇÕES MATRICIAIS ───────────────

// Determinante de matriz 2x2
function det_2x2(A) {
    if (len(A) != 2 || len(A[0]) != 2) {
        return 0.0;  // Matriz inválida
    }
    return A[0][0] * A[1][1] - A[0][1] * A[1][0];
}

// Determinante de matriz 3x3 (regra de Sarrus)
function det_3x3(A) {
    if (len(A) != 3 || len(A[0]) != 3) {
        return 0.0;  // Matriz inválida
    }
    
    var d = A[0][0] * A[1][1] * A[2][2] +
            A[0][1] * A[1][2] * A[2][0] +
            A[0][2] * A[1][0] * A[2][1] -
            A[0][2] * A[1][1] * A[2][0] -
            A[0][0] * A[1][2] * A[2][1] -
            A[0][1] * A[1][0] * A[2][2];
    
    return d;
}

// Traço da matriz (soma da diagonal)
function trace(A) {
    if (len(A) == 0) return 0.0;
    
    var result = 0.0;
    var i = 0;
    while (i < len(A)) {
        if (i < len(A[i])) {
            result = result + A[i][i];
        }
        i = i + 1;
    }
    return result;
}

// ─────────────── HELPER FUNCTIONS ───────────────

function abs_VECTORS(x) {
    if (x < 0.0) return -x;
    return x;
}
```

**Pontos positivos:**
- Todas as funções implementadas completamente
- Tratamento de casos especiais (tamanho diferente, matriz nula, etc)
- Comentários explicativos
- Constantes nomeadas
- Seções bem organizadas
- Sem stubs

---

## 🔍 TIPOS DE ERRO E COMO TRATAR

### Divisão por Zero

```sapphire
// ❌ RUIM
function inverse_scalar(x) {
    return 1.0 / x;  // Explode se x == 0
}

// ✅ BOM
function inverse_scalar(x) {
    if (abs(x) < EPS) {
        return 0.0;  // Ou returnar erro, ou NaN, ou constante
    }
    return 1.0 / x;
}
```

### Índice Fora do Limite

```sapphire
// ❌ RUIM
function get_element(arr, i) {
    return arr[i];  // Crash se i >= len(arr)
}

// ✅ BOM
function get_element(arr, i) {
    if (i < 0 || i >= len(arr)) {
        return 0.0;  // Ou retorna valor padrão
    }
    return arr[i];
}
```

### Entrada Inválida

```sapphire
// ❌ RUIM
function norm(v) {
    var sum = 0.0;
    var i = 0;
    while (i < len(v)) {
        sum = sum + v[i] * v[i];
        i = i + 1;
    }
    return sqrt(sum);
}

// ✅ BOM
function norm(v) {
    if (v == null || len(v) == 0) {
        return 0.0;
    }
    
    var sum = 0.0;
    var i = 0;
    while (i < len(v)) {
        sum = sum + v[i] * v[i];
        i = i + 1;
    }
    return sqrt(sum);
}
```

---

## 📝 NOMES DE VARIÁVEIS

### ❌ RUIM
```sapphire
var x, y, z, temp, data, result, val, num
```

### ✅ BOM
```sapphire
var matrix_A, vector_b, iteration_count
var sum_squared_errors, learning_rate
var is_converged, max_iterations
```

---

## 🎯 RESUMO FINAL

**Antes de enviar qualquer código:**

1. ✅ Nenhuma função é stub?
2. ✅ Todos os casos especiais foram tratados?
3. ✅ Tem constantes nomeadas?
4. ✅ Tem seções com demarcadores?
5. ✅ Tem comentários nos algoritmos?
6. ✅ Nomes descritivos?
7. ✅ Sem magic numbers?
8. ✅ Sem `for` loops?
9. ✅ Sem TODO/FIXME?
10. ✅ Sem código comentado?

**Se qualquer item é ❌, volte e conserte antes de enviar.**

---

**Criado por Claude Sonnet 4.5 — 2026-08-03**
**Para o projeto Sapphire**
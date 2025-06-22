#include "evaluator.h"
#include <iostream>
#include <stdexcept>
#include <cmath>

// --- Métodos de Execução ---

void Evaluator::evaluate(const std::vector<std::unique_ptr<Stmt>>& statements) {
    try {
        for (const auto& stmt : statements) {
            if (stmt) { // Garante que não vamos executar um statement nulo
                execute(*stmt);
            }
        }
    } catch (const std::runtime_error& error) {
        std::cerr << "Erro de Runtime: " << error.what() << std::endl;
    }
}

void Evaluator::execute(Stmt& stmt) {
    stmt.accept(*this);
}

double Evaluator::evaluate_expr(Expr& expr) {
    return expr.accept(*this);
}

// Em Sapphire, nil e false são falsos. Todo o resto é verdadeiro.
// Para números, 0 é falso.
bool Evaluator::is_truthy(double value) {
    return value != 0.0;
}

// --- Implementação dos Visitors de Statement ---

void Evaluator::visit_expression_stmt(ExpressionStmt& stmt) {
    evaluate_expr(*stmt.expression); // Avalia e descarta o resultado
}

void Evaluator::visit_var_stmt(VarStmt& stmt) {
    double value = 0.0; // Valor padrão é 0 (representando 'nil'/'false')
    if (stmt.initializer) {
        value = evaluate_expr(*stmt.initializer);
    }
    environment[stmt.name.literal] = value;
}

void Evaluator::visit_print_stmt(PrintStmt& stmt) {
    double value = evaluate_expr(*stmt.expression);
    std::cout << value << std::endl;
}

void Evaluator::visit_if_stmt(IfStmt& stmt) {
    if (is_truthy(evaluate_expr(*stmt.condition))) {
        execute(*stmt.then_branch);
    } else if (stmt.else_branch) {
        execute(*stmt.else_branch);
    }
}

void Evaluator::visit_block_stmt(BlockStmt& stmt) {
    // Futuramente, aqui controlaremos escopos de variáveis.
    // Por enquanto, apenas executamos a lista de statements.
    evaluate(stmt.statements);
}

// --- Implementação dos Visitors de Expressão ---

double Evaluator::visit_assign_expr(AssignExpr& expr) {
    double value = evaluate_expr(*expr.value);
    if (environment.count(expr.name.literal)) {
        environment[expr.name.literal] = value;
        return value;
    }
    throw std::runtime_error("Variavel nao definida para atribuicao: '" + expr.name.literal + "'.");
}

double Evaluator::visit_literal_expr(Literal& expr) {
    // O Parser já converteu true/false/nil para 1.0 ou 0.0
    if (expr.value_token.type == TokenType::NUMBER) {
        return std::stod(expr.value_token.literal);
    }
    // Para TRUE, FALSE, NIL
    return std::stod(expr.value_token.literal);
}

double Evaluator::visit_grouping_expr(Grouping& expr) {
    return evaluate_expr(*expr.expression);
}

double Evaluator::visit_variable_expr(Variable& expr) {
    if (environment.count(expr.name.literal)) {
        return environment.at(expr.name.literal);
    }
    throw std::runtime_error("Variavel nao definida: '" + expr.name.literal + "'.");
}

double Evaluator::visit_unary_expr(Unary& expr) {
    double right = evaluate_expr(*expr.right);
    switch (expr.op.type) {
        case TokenType::MINUS: return -right;
        case TokenType::BANG: return is_truthy(right) ? 0.0 : 1.0;
    }
    return 0.0; // Inalcançável
}

double Evaluator::visit_logical_expr(LogicalExpr& expr) {
    double left = evaluate_expr(*expr.left);
    if (expr.op.type == TokenType::OR) {
        if (is_truthy(left)) return left;
    } else { // AND
        if (!is_truthy(left)) return left;
    }
    return evaluate_expr(*expr.right);
}

double Evaluator::visit_binary_expr(Binary& expr) {
    double left = evaluate_expr(*expr.left);
    double right = evaluate_expr(*expr.right);
    switch (expr.op.type) {
        // Aritmética
        case TokenType::PLUS: return left + right;
        case TokenType::MINUS: return left - right;
        case TokenType::STAR: return left * right;
        case TokenType::SLASH:
            if (right == 0) throw std::runtime_error("Divisao por zero.");
            return left / right;
        // Comparações
        case TokenType::GREATER: return left > right ? 1.0 : 0.0;
        case TokenType::GREATER_EQUAL: return left >= right ? 1.0 : 0.0;
        case TokenType::LESS: return left < right ? 1.0 : 0.0;
        case TokenType::LESS_EQUAL: return left <= right ? 1.0 : 0.0;
        case TokenType::EQUAL_EQUAL: return left == right ? 1.0 : 0.0;
        case TokenType::BANG_EQUAL: return left != right ? 1.0 : 0.0;
    }
    return 0.0; // Inalcançável
}
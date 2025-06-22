#ifndef SAPPHIRE_EVALUATOR_H
#define SAPPHIRE_EVALUATOR_H

#include "ast.h"
#include <map>
#include <string>
#include <vector>
#include <memory>

class Evaluator : public ExprVisitor, public StmtVisitor {
public:
    void evaluate(const std::vector<std::unique_ptr<Stmt>>& statements);

private:
    std::map<std::string, double> environment;

    void execute(Stmt& stmt);
    double evaluate_expr(Expr& expr);
    bool is_truthy(double value);

    // Métodos para Statements
    void visit_expression_stmt(ExpressionStmt& stmt) override;
    void visit_var_stmt(VarStmt& stmt) override;
    void visit_print_stmt(PrintStmt& stmt) override;
    void visit_if_stmt(IfStmt& stmt) override;
    void visit_block_stmt(BlockStmt& stmt) override;

    // Métodos para Expressões
    double visit_binary_expr(Binary& expr) override;
    double visit_logical_expr(LogicalExpr& expr) override;
    double visit_grouping_expr(Grouping& expr) override;
    double visit_literal_expr(Literal& expr) override;
    double visit_variable_expr(Variable& expr) override;
    double visit_unary_expr(Unary& expr) override;
    double visit_assign_expr(AssignExpr& expr) override;
};

#endif // SAPPHIRE_EVALUATOR_H
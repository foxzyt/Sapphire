#include "ast.h"
Binary::Binary(std::unique_ptr<Expr> l, Token o, std::unique_ptr<Expr> r) : left(std::move(l)), op(o), right(std::move(r)) {}
double Binary::accept(ExprVisitor& v) { return v.visit_binary_expr(*this); }
LogicalExpr::LogicalExpr(std::unique_ptr<Expr> l, Token o, std::unique_ptr<Expr> r) : left(std::move(l)), op(o), right(std::move(r)) {}
double LogicalExpr::accept(ExprVisitor& v) { return v.visit_logical_expr(*this); }
Grouping::Grouping(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
double Grouping::accept(ExprVisitor& v) { return v.visit_grouping_expr(*this); }
Literal::Literal(Token v) : value_token(std::move(v)) {}
double Literal::accept(ExprVisitor& v) { return v.visit_literal_expr(*this); }
Variable::Variable(Token n) : name(std::move(n)) {}
double Variable::accept(ExprVisitor& v) { return v.visit_variable_expr(*this); }
Unary::Unary(Token o, std::unique_ptr<Expr> r) : op(o), right(std::move(r)) {}
double Unary::accept(ExprVisitor& v) { return v.visit_unary_expr(*this); }
AssignExpr::AssignExpr(Token n, std::unique_ptr<Expr> v) : name(std::move(n)), value(std::move(v)) {}
double AssignExpr::accept(ExprVisitor& v) { return v.visit_assign_expr(*this); }
ExpressionStmt::ExpressionStmt(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
void ExpressionStmt::accept(StmtVisitor& v) { v.visit_expression_stmt(*this); }
VarStmt::VarStmt(Token n, std::unique_ptr<Expr> i) : name(std::move(n)), initializer(std::move(i)) {}
void VarStmt::accept(StmtVisitor& v) { v.visit_var_stmt(*this); }
PrintStmt::PrintStmt(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
void PrintStmt::accept(StmtVisitor& v) { v.visit_print_stmt(*this); }
BlockStmt::BlockStmt(std::vector<std::unique_ptr<Stmt>> s) : statements(std::move(s)) {}
void BlockStmt::accept(StmtVisitor& v) { v.visit_block_stmt(*this); }
IfStmt::IfStmt(std::unique_ptr<Expr> c,std::unique_ptr<Stmt> t,std::unique_ptr<Stmt> e) : condition(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
void IfStmt::accept(StmtVisitor& v) { v.visit_if_stmt(*this); }
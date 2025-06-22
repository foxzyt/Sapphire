#ifndef SAPPHIRE_AST_H
#define SAPPHIRE_AST_H

#include "tokens.h"
#include <memory>
#include <vector>
#include <string>

// Forward Declarations para que as classes se "conheçam"
struct Expr; struct Binary; struct LogicalExpr; struct Grouping; struct Literal; struct Variable; struct Unary; struct AssignExpr;
struct Stmt; struct ExpressionStmt; struct VarStmt; struct PrintStmt; struct IfStmt; struct BlockStmt;

// O contrato para qualquer "visitante" de Expressões
class ExprVisitor {
public:
    virtual double visit_binary_expr(Binary& expr) = 0;
    virtual double visit_logical_expr(LogicalExpr& expr) = 0;
    virtual double visit_grouping_expr(Grouping& expr) = 0;
    virtual double visit_literal_expr(Literal& expr) = 0;
    virtual double visit_variable_expr(Variable& expr) = 0;
    virtual double visit_unary_expr(Unary& expr) = 0;
    virtual double visit_assign_expr(AssignExpr& expr) = 0;
    virtual ~ExprVisitor() = default;
};

// A classe base para todas as Expressões
struct Expr {
    virtual double accept(ExprVisitor& visitor) = 0;
    virtual ~Expr() = default;
};

// O contrato para qualquer "visitante" de Statements
class StmtVisitor {
public:
    virtual void visit_expression_stmt(ExpressionStmt& stmt) = 0;
    virtual void visit_var_stmt(VarStmt& stmt) = 0;
    virtual void visit_print_stmt(PrintStmt& stmt) = 0;
    virtual void visit_if_stmt(IfStmt& stmt) = 0;
    virtual void visit_block_stmt(BlockStmt& stmt) = 0;
    virtual ~StmtVisitor() = default;
};

// A classe base para todos os Statements
struct Stmt {
    virtual void accept(StmtVisitor& visitor) = 0;
    virtual ~Stmt() = default;
};


// --- Declarações das Classes Concretas ---
// (As implementações ficarão no ast.cpp)

struct Binary : Expr {
    std::unique_ptr<Expr> left; Token op; std::unique_ptr<Expr> right;
    Binary(std::unique_ptr<Expr> l, Token o, std::unique_ptr<Expr> r);
    double accept(ExprVisitor& v) override;
};
struct LogicalExpr : Expr {
    std::unique_ptr<Expr> left; Token op; std::unique_ptr<Expr> right;
    LogicalExpr(std::unique_ptr<Expr> l, Token o, std::unique_ptr<Expr> r);
    double accept(ExprVisitor& v) override;
};
struct Grouping : Expr {
    std::unique_ptr<Expr> expression;
    Grouping(std::unique_ptr<Expr> e);
    double accept(ExprVisitor& v) override;
};
struct Literal : Expr {
    Token value_token;
    Literal(Token v);
    double accept(ExprVisitor& v) override;
};
struct Variable : Expr {
    Token name;
    Variable(Token n);
    double accept(ExprVisitor& v) override;
};
struct Unary : Expr {
    Token op; std::unique_ptr<Expr> right;
    Unary(Token o, std::unique_ptr<Expr> r);
    double accept(ExprVisitor& v) override;
};
struct AssignExpr : Expr {
    Token name; std::unique_ptr<Expr> value;
    AssignExpr(Token n, std::unique_ptr<Expr> v);
    double accept(ExprVisitor& v) override;
};

struct ExpressionStmt : Stmt {
    std::unique_ptr<Expr> expression;
    ExpressionStmt(std::unique_ptr<Expr> e);
    void accept(StmtVisitor& v) override;
};
struct VarStmt : Stmt {
    Token name; std::unique_ptr<Expr> initializer;
    VarStmt(Token n, std::unique_ptr<Expr> i);
    void accept(StmtVisitor& v) override;
};
struct PrintStmt : Stmt {
    std::unique_ptr<Expr> expression;
    PrintStmt(std::unique_ptr<Expr> e);
    void accept(StmtVisitor& v) override;
};
struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
    BlockStmt(std::vector<std::unique_ptr<Stmt>> s);
    void accept(StmtVisitor& v) override;
};
struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition; std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch;
    IfStmt(std::unique_ptr<Expr> c, std::unique_ptr<Stmt> t, std::unique_ptr<Stmt> e);
    void accept(StmtVisitor& v) override;
};

#endif // SAPPHIRE_AST_H
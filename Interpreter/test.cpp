#include <cctype>
#include <cstdio>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace std;

namespace {
template <class T, class... Args>
static
    typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type
    make_unique(Args &&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
enum Token {
    tok_eof = -1,
    tok_identifier = -2,
    tok_number = -3,
    tok_begin = -4,
    tok_end = -5,
    tok_read = -6,
    tok_write = -7,
    tok_integer = -8,
    tok_if = -9,
    tok_else = -10,
    tok_then = -11,
    tok_function = -12,
    tok_neq = -13,     // <>
    tok_eq = -14,      // =
    tok_less = -15,    // <
    tok_greater = -16, // >
    tok_assign = -17,  // :=
    tok_mul = -18,     // *
    tok_plus = -19,    // +
    tok_minus = -20,   // -
    tok_le = -21,      // <=
    tok_ge = -22,      // >=
    tok_lp = -23,      // (
    tok_rp = -24,      // )
    tok_semi = -25,    // ;
    tok_comma = -26    // ,
};

string IdentifierStr;
int numVal;

unordered_map<string, int> reserveTokens = {
    {"function", tok_function}, {"integer", tok_integer}, {"if", tok_if},
    {"else", tok_else},         {"then", tok_then},       {"read", tok_read},
    {"write", tok_write},       {"begin", tok_begin},     {"end", tok_end}};

unordered_map<int, int> binopPrecedence = {
    {tok_plus, 20},    {tok_minus, 20}, {tok_mul, 40}, {tok_less, 10},
    {tok_greater, 10}, {tok_neq, 9},    {tok_le, 10},  {tok_ge, 10},
    {tok_assign, 5},   {tok_eq, 9}};

struct ExprAST;

struct Context {
    int ret_;
    vector<int> args_;
    unordered_map<string, int> variables_;
    shared_ptr<Context> subContext_;
    unordered_map<string, ExprAST *> funcs_;
};

shared_ptr<Context> gContext;
shared_ptr<Context> cContext;

void Error(string err) {
    cerr << err << endl;
    terminate();
}

int gettok() {
    static int lastChar = ' ';

    while (isspace(lastChar))
        lastChar = getchar();

    if (isdigit(lastChar)) {
        string numStr;
        do {
            numStr += lastChar;
            lastChar = getchar();
        } while (isdigit(lastChar));
        numVal = stoi(numStr);
        return tok_number;
    }

    if (isalpha(lastChar)) {
        IdentifierStr = lastChar;
        while (isalnum((lastChar = getchar())))
            IdentifierStr += lastChar;

        auto it = reserveTokens.find(IdentifierStr);
        if (it != reserveTokens.end()) {
            return it->second;
        }

        return tok_identifier;
    }

    int thisChar = getchar();

    switch (lastChar) {
    case EOF:
        return tok_eof;
    case '<':
        if (thisChar == '>') {
            lastChar = getchar();
            return tok_neq;
        } else if (thisChar == '=') {
            lastChar = getchar();
            return tok_le;
        } else {
            lastChar = thisChar;
            return tok_less;
        }
    case '>':
        if (thisChar == '=') {
            lastChar = getchar();
            return tok_ge;
        } else {
            lastChar = thisChar;
            return tok_greater;
        }
    case ':':
        if (thisChar == '=') {
            lastChar = getchar();
            return tok_assign;
        } else {
            Error("bad : without =");
        }
    case '+':
        lastChar = thisChar;
        return tok_plus;
    case '-':
        lastChar = thisChar;
        return tok_minus;
    case '*':
        lastChar = thisChar;
        return tok_mul;
    case '(':
        lastChar = thisChar;
        return tok_lp;
    case ')':
        lastChar = thisChar;
        return tok_rp;
    case ';':
        lastChar = thisChar;
        return tok_semi;
    case ',':
        lastChar = thisChar;
        return tok_comma;
    case '=':
        lastChar = thisChar;
        return tok_eq;
    }

    Error("bad char " + to_string(lastChar));
    return 0;
}

class clean_ {
public:
    clean_(const clean_ &) = delete;

    clean_(clean_ &&) = delete;

    clean_ &operator=(const clean_ &) = delete;

    clean_() = default;

    ~clean_() = default;
};

class DeferCaller final : clean_ {
public:
    DeferCaller(std::function<void()> &&functor)
        : functor_(std::move(functor)) {}

    DeferCaller(const std::function<void()> &functor) : functor_(functor) {}

    ~DeferCaller() {
        if (functor_)
            functor_();
    }

    void cancel() { functor_ = nullptr; }

private:
    std::function<void()> functor_;
};

class ExprAST {
public:
    virtual ~ExprAST() {}
    virtual void interpret() {}
    virtual int value() { return 0; }
    virtual string name() { return ""; }
};

class NumberExprAST : public ExprAST {
public:
    NumberExprAST(int val) : val_(val) {}

    int value() override { return val_; }

private:
    int val_;
};

class VariableExprAST : public ExprAST {
public:
    VariableExprAST(const string &name) : name_(name) {}

    void interpret() override {
        auto it = cContext->variables_.find(name_);
        if (it == cContext->variables_.end()) {
            cContext->variables_[name_] = 0;
        }
    }

    int value() override { return cContext->variables_[name_]; }

    string name() override { return name_; }

private:
    string name_;
};

class BinaryExprAST : public ExprAST {
public:
    BinaryExprAST(int op, unique_ptr<ExprAST> lhs, unique_ptr<ExprAST> rhs)
        : op_(op), lhs_(move(lhs)), rhs_(move(rhs)) {}

    string name() override {
        if (rhs_) {
            Error("无法获得该二元式的名称");
        }
        return lhs_->name();
    }

    void interpret() override {
        switch (op_) {
        default:
            Error("bad op_ " + to_string(op_));
        case tok_semi:
            lhs_->interpret();
            rhs_->interpret();
            break;
        case tok_assign: {
            auto it = cContext->variables_.find(lhs_->name());
            if (it == cContext->variables_.end())
                Error("无法找到变量 " + lhs_->name());
            it->second = rhs_->value();
            break;
        }
        case tok_neq:
        case tok_eq:
        case tok_less:
        case tok_greater:
        case tok_mul:
        case tok_plus:
        case tok_minus:
        case tok_le:
        case tok_ge:
            break;
        }
    }

    int value() override {
        if (rhs_ == nullptr)
            return lhs_->value();

        switch (op_) {
        default:
            Error("bad op_ " + to_string(op_));
        case tok_assign: {
            auto it = cContext->variables_.find(lhs_->name());
            if (it == cContext->variables_.end())
                Error("无法找到变量 " + lhs_->name());
            return it->second = rhs_->value();
        }
        case tok_neq:
            return lhs_->value() != rhs_->value();
        case tok_eq:
            return lhs_->value() == rhs_->value();
        case tok_less:
            return lhs_->value() < rhs_->value();
        case tok_greater:
            return lhs_->value() > rhs_->value();
        case tok_mul:
            return lhs_->value() * rhs_->value();
        case tok_plus:
            return lhs_->value() + rhs_->value();
        case tok_minus:
            return lhs_->value() - rhs_->value();
        case tok_le:
            return lhs_->value() <= rhs_->value();
        case tok_ge:
            return lhs_->value() >= rhs_->value();
        }
    }

private:
    int op_;
    unique_ptr<ExprAST> lhs_, rhs_;
};

class ConditionExprAST : public ExprAST {
public:
    ConditionExprAST(unique_ptr<ExprAST> cond, unique_ptr<ExprAST> then,
                     unique_ptr<ExprAST> else_)
        : cond_(move(cond)), then_(move(then)), else_(move(else_)) {}

    void interpret() override {
        if (cond_->value()) {
            then_->interpret();
        } else {
            else_->interpret();
        }
    }

private:
    unique_ptr<ExprAST> cond_;
    unique_ptr<ExprAST> then_;
    unique_ptr<ExprAST> else_;
};

class CallExprAST : public ExprAST {
public:
    CallExprAST(const string &callee, vector<unique_ptr<ExprAST>> args)
        : callee_(callee), args_(move(args)) {}

    void interpret() override {
        auto funcIt = cContext->funcs_.find(callee_);
        if (funcIt == cContext->funcs_.end()) {
            Error("没有名为 " + callee_ + " 的函数");
        }

        cContext->subContext_ = make_shared<Context>();
        DeferCaller caller([] { cContext->subContext_.reset(); });

        for (auto &arg : args_) {
            cContext->subContext_->args_.push_back(arg->value());
        }

        auto savedContext = cContext;
        cContext = cContext->subContext_;

        funcIt->second->value();

        cContext = savedContext;
    }

    int value() override {
        auto funcIt = cContext->funcs_.find(callee_);
        if (funcIt == cContext->funcs_.end()) {
            Error("没有名为 " + callee_ + " 的函数");
        }

        cContext->subContext_ = make_shared<Context>();
        DeferCaller caller([] { cContext->subContext_.reset(); });

        for (auto &arg : args_) {
            cContext->subContext_->args_.push_back(arg->value());
        }

        auto savedContext = cContext;
        cContext = cContext->subContext_;

        int ret;
        cContext->variables_[callee_] = 0;
        funcIt->second->value();
        ret = cContext->variables_[callee_];

        cContext = savedContext;

        return ret;
    }

private:
    string callee_;
    vector<unique_ptr<ExprAST>> args_;
};

class PrototypeAST {
public:
    PrototypeAST(const string &name, vector<string> args)
        : name_(name), args_(move(args)) {}

    void interpret(ExprAST *ast) { cContext->funcs_[name_] = ast; }

    void value() {
        if (args_.size() != cContext->args_.size()) {
            Error("形参与实参不匹配");
        }
        for (int i = 0; i < args_.size(); i++) {
            cContext->variables_[args_[i]] = cContext->args_[i];
        }
    }

private:
    string name_;
    vector<string> args_;
};

class FunctionAST : public ExprAST {
public:
    FunctionAST(unique_ptr<PrototypeAST> proto, unique_ptr<ExprAST> body)
        : proto_(move(proto)), body_(move(body)) {}

    int value() override {
        proto_->interpret(this);
        proto_->value();
        body_->interpret();
        return 0;
    }

    void interpret() override { proto_->interpret(this); }

private:
    unique_ptr<PrototypeAST> proto_;
    unique_ptr<ExprAST> body_;
};

class ReadAST : public ExprAST {
public:
    ReadAST(const string &name) : name_(name) {}

    void interpret() override {
        int i;
        cin >> i;
        cContext->variables_[name_] = i;
    }

private:
    string name_;
};

class WriteAST : public ExprAST {
public:
    WriteAST(const string &name) : name_(name) {}

    void interpret() override {
        auto it = cContext->variables_.find(name_);
        if (it == cContext->variables_.end()) {
            Error("找不到变量 " + name_);
        }

        cout << it->second << endl;
    }

private:
    string name_;
};

int curTok;

int getNextToken() { return curTok = gettok(); }

int getTokPrecedence() { return binopPrecedence[curTok]; }

unique_ptr<ExprAST> logError(const string &err) {
    cerr << "Error: " << err << endl;
    return nullptr;
}

unique_ptr<ExprAST> parseExpression();

unique_ptr<ExprAST> parseNumberExpr() {
    auto ret = make_unique<NumberExprAST>(numVal);
    getNextToken();
    return move(ret);
}

unique_ptr<ExprAST> parseParenExpr() {
    getNextToken();
    auto ret = parseExpression();
    if (!ret)
        return nullptr;
    if (curTok != tok_rp)
        return logError("expected ')'");
    getNextToken();
    return move(ret);
}

unique_ptr<ExprAST> parseExpressions() {
    auto lhs = parseExpression();
    if (curTok != tok_semi) {
        return lhs;
    } else {
        getNextToken();
        return make_unique<BinaryExprAST>(tok_semi, move(lhs),
                                          move(parseExpressions()));
    }
}

unique_ptr<ExprAST> parseFunctionExpr() {
    getNextToken();
    if (curTok != tok_identifier) {
        return logError("需要函数名称");
    }
    string name = IdentifierStr;

    vector<string> args;

    getNextToken();
    if (curTok != tok_lp) {
        return logError("需要'('");
    }

    getNextToken();
    if (curTok != tok_rp) {
        while (1) {
            if (curTok != tok_identifier) {
                return logError("需要函数参数名称");
            }
            args.push_back(move(IdentifierStr));

            getNextToken();
            if (curTok == tok_rp) {
                break;
            }

            if (curTok != tok_comma) {
                return logError("需要')'或者','");
            }
            getNextToken();
        }
    }

    getNextToken();

    if (curTok != tok_semi) {
        return logError("需要';'");
    }

    auto proto = make_unique<PrototypeAST>(name, args);
    unique_ptr<ExprAST> body;

    getNextToken();

    if (curTok != tok_begin) {
        return logError("需要begin");
    }

    getNextToken();
    if (curTok != tok_end) {
        body = move(parseExpressions());
    }

    getNextToken();
    return make_unique<FunctionAST>(move(proto), move(body));
}

unique_ptr<ExprAST> parseIdentifierExpr() {
    string name = IdentifierStr;
    getNextToken();

    if (curTok != tok_lp)
        return make_unique<VariableExprAST>(name);

    getNextToken();
    vector<unique_ptr<ExprAST>> args;
    if (curTok != tok_rp) {
        while (1) {
            auto arg = parseExpression();
            if (arg)
                args.push_back(move(arg));
            else
                return nullptr;

            if (curTok == tok_rp)
                break;

            if (curTok != tok_comma)
                return logError("在参数列表中需要')'或者','");

            getNextToken();
        }
    }

    getNextToken();

    return make_unique<CallExprAST>(name, move(args));
}

unique_ptr<ExprAST> parseInteger() {
    getNextToken();
    switch (curTok) {
    case tok_identifier:
        return parseIdentifierExpr();
    case tok_function:
        return parseFunctionExpr();
    default:
        return logError("未知token");
    }
}

unique_ptr<ExprAST> parseReadAST() {
    getNextToken();
    if (curTok != tok_lp) {
        return logError("需要'('");
    }
    getNextToken();
    if (curTok != tok_identifier) {
        return logError("需要标识符");
    }
    string name = IdentifierStr;
    getNextToken();
    if (curTok != tok_rp) {
        return logError("需要')'");
    }
    getNextToken();
    return make_unique<ReadAST>(name);
}

unique_ptr<ExprAST> parseWriteAST() {
    getNextToken();
    if (curTok != tok_lp) {
        return logError("需要'('");
    }
    getNextToken();
    if (curTok != tok_identifier) {
        return logError("需要标识符");
    }
    string name = IdentifierStr;
    getNextToken();
    if (curTok != tok_rp) {
        return logError("需要')'");
    }
    getNextToken();
    return make_unique<WriteAST>(name);
}

unique_ptr<ExprAST> parseConditionExpr() {
    getNextToken();
    auto condition = parseExpression();
    if (curTok != tok_then) {
        return logError("需要then");
    }
    getNextToken();
    auto then = parseExpression();
    if (curTok != tok_else) {
        return logError("需要else");
    }
    getNextToken();
    auto else_ = parseExpression();

    return make_unique<ConditionExprAST>(move(condition), move(then),
                                         move(else_));
}

unique_ptr<ExprAST> parsePrimary() {
    switch (curTok) {
    default:
        return logError("未知token " + to_string(curTok));
    case tok_read:
        // cout << "解析ReadAST" << endl;
        return parseReadAST();
    case tok_write:
        // cout << "解析WriteAST" << endl;
        return parseWriteAST();
    case tok_integer:
        // cout << "解析integer" << endl;
        return parseInteger();
    case tok_identifier:
        // cout << "解析标识符" << endl;
        return parseIdentifierExpr();
    case tok_if:
        // cout << "解析条件语句" << endl;
        return parseConditionExpr();
    case tok_lp:
        // cout << "解析括号" << endl;
        return parseParenExpr();
    case tok_number:
        // cout << "解析到数字" << endl;
        return parseNumberExpr();
    }
}

unique_ptr<ExprAST> parseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> lhs) {
    while (1) {
        int tokPrec = getTokPrecedence();

        if (tokPrec < ExprPrec)
            return lhs;

        int binOp = curTok;
        getNextToken();

        auto rhs = parsePrimary();
        if (!rhs)
            return nullptr;

        int nextPrec = getTokPrecedence();
        if (tokPrec < nextPrec) {
            rhs = parseBinOpRHS(tokPrec + 1, move(rhs));
            if (!rhs)
                return nullptr;
        }

        lhs = make_unique<BinaryExprAST>(binOp, move(lhs), move(rhs));
    }
}

unique_ptr<ExprAST> parseExpression() {
    auto lhs = parsePrimary();
    if (!lhs)
        return nullptr;

    return parseBinOpRHS(1, move(lhs));
}

void handleProcedure() {
    getNextToken();
    auto p = move(parseExpressions());
    if (p) {
        if (curTok == tok_end) {
            // cout << "解析到一个过程" << endl;
            cContext = make_shared<Context>();
            p->interpret();
            // getNextToken();
        } else {
            logError("需要end");
        }
    } else {
        getNextToken();
    }
}

void runMainLoop() {
    while (printf("ready> "), getNextToken(), 1) {
        switch (curTok) {
        case tok_semi:
            getNextToken();
            break;
        case tok_begin:
            handleProcedure();
            break;
        }
    }
}
}

int main(void) {
    runMainLoop();
    return 0;
}

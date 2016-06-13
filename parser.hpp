
#ifndef _PARSER_HPP_
#define _PARSER_HPP_

#define OPTYPE_binop 1
#define OPTYPE_colmn 2
#define OPTYPE_const 3

#define OPTYPE_mul 1
#define OPTYPE_div 2
#define OPTYPE_add 3
#define OPTYPE_sub 4
#define OPTYPE_lt 5
#define OPTYPE_le 6
#define OPTYPE_eq 7
#define OPTYPE_ne 8
#define OPTYPE_gt 9
#define OPTYPE_ge 10
#define OPTYPE_and 11
#define OPTYPE_or 12

static int OperatorType(std::string str);

class Operation {
public:
    virtual ~Operation() {}
    virtual int Type() { return 0; }
};

class ConstantOperation : public Operation {
public:
    double value;
     ConstantOperation(double val) : value(val) { }
    virtual int Type() { return OPTYPE_const; }
};

class ColumnOperation : public Operation {
public:
    std::string name;
    Column *column;
    ColumnOperation(const std::string &name) : name(name), column(NULL) {}
    virtual int Type() { return OPTYPE_colmn; }
};

class BinaryOperation : public Operation {
public:
    std::string op;
    int optype;
    Operation* LHS, *RHS;
    BinaryOperation(Operation *LHS, Operation *RHS, std::string op) : op(op), optype(OperatorType(op)), LHS(LHS), RHS(RHS) {} 
    virtual int Type() { return OPTYPE_binop; }
};

struct ColumnList {
    Column *column;
    ColumnList *next;
};
//scans the relevant columns from an operation
static ColumnList* GetColumns(Table *table, Operation *op); 

struct OperationList {
    Operation *operation;
    ColumnList *columns; //relevant columns to the operation
    OperationList *next;
};

struct Query {
    OperationList *selection;
    char *table;
    OperationList *where;
};

enum Token {
    tok_select = 1,
    tok_from = 2,
    tok_where = 3,
    tok_operator = 4,
    tok_constant = 5,
    tok_identifier = 6,
    tok_leftparen = 7,
    tok_rightparen = 8,
    tok_comma = 9,
    tok_invalid = 126,
    tok_eof = 127
};

static std::string strval;
static double numval;

static bool IsOperator(char c) {
    switch(c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '>':
        case '<':
        case '=':
        case '!':
        case '|':
        case '&':
            return true;
    }
    return false;
}

static void _PrintOperation(Operation* operation) {
    if (!operation) return;
    if (operation->Type() == OPTYPE_binop) {
        BinaryOperation *binop = (BinaryOperation*) operation;
        std::cout << "(";
        _PrintOperation(binop->LHS);
        std::cout << " " << binop->op << " ";
        _PrintOperation(binop->RHS);
        std::cout << ")";
        return;
    }
    if (operation->Type() == OPTYPE_colmn) {
        ColumnOperation *colop = (ColumnOperation*) operation;
        std::cout << colop->name;
        return;
    }
    if (operation->Type() == OPTYPE_const) {
        ConstantOperation *constop = (ConstantOperation*) operation;
        std::cout << constop->value;
        return;
    }
}

static void PrintOperation(Operation* operation) {
    // print a chain of operations, to check if it has been parsed correctly
    _PrintOperation(operation);
    std::cout << std::endl << std::flush;
}

static std::string TokToString(int token) {
    // token to string, for error messages
    switch(token) {
        case tok_select: return "SELECT";
        case tok_from: return "FROM";
        case tok_where: return "WHERE";
        case tok_operator: return "OPERATOR";
        case tok_leftparen: return "(";
        case tok_rightparen: return ")";
        case tok_identifier: return "IDENTIFIER";
        case tok_constant: return "CONSTANT";
        case tok_comma: return ",";
        case tok_eof: return ";";
        case tok_invalid: return "INVALID";
        default: return "TOKEN";
    }
}

static int OperatorType(std::string str) {
    // same operator precedence as C, higher = evaluated first
    // we separate operators by 100 so there is some room in between
    if (str == "/") return OPTYPE_div;
    if (str == "*") return OPTYPE_mul;
    if (str == "+") return OPTYPE_add;
    if (str == "-") return OPTYPE_sub;
    if (str == ">=") return OPTYPE_ge;
    if (str == ">") return OPTYPE_gt;
    if (str == "<") return OPTYPE_lt;
    if (str == "<=") return OPTYPE_le;
    if (str == "==") return OPTYPE_eq;
    if (str == "!=") return OPTYPE_ne;
    if (str == "<>") return OPTYPE_ne;
    if (str == "&&") return OPTYPE_and;
    if (str == "AND") return OPTYPE_and;
    if (str == "||") return OPTYPE_or;
    if (str == "OR") return OPTYPE_or;
    return -1;
}

static int OperatorPrecedence(std::string str) {
    // same operator precedence as C, higher = evaluated first
    // we separate operators by 100 so there is some room in between
    if (str == "/") return 1200;
    if (str == "*") return 1200;
    if (str == "+") return 1100;
    if (str == "-") return 1100;
    if (str == ">=") return 700;
    if (str == ">") return 700;
    if (str == "<") return 700;
    if (str == "<=") return 700;
    if (str == "==") return 600;
    if (str == "!=") return 600;
    if (str == "<>") return 600;
    if (str == "&&") return 400;
    if (str == "AND") return 400;
    if (str == "||") return 300;
    if (str == "OR") return 300;
    return -1;
}

static bool IsOperator(std::string str) {
    return OperatorPrecedence(str) > 0;
}

static Token _ParseToken(std::string query, size_t& index) {
    // parse a token, incrementing the current index in the string as we go
    while(isspace(query[index])) { //ignore all spaces
        index++;
    }

    // ; or eof signals the end of the query
    if (index >= query.size() || query[index] == ';') return tok_eof;

    if (isalpha(query[index])) { //identifiers must start with an alphabetic character (can't start with numbers)
        strval = query[index++];
        while(isalnum(query[index]))  {
            strval += query[index++];
        }
        // scan until the current character is no longer a alphabetic character or number

        // handle special strings
        if (strval == "SELECT") {
            return tok_select;
        }
        if (strval == "FROM") {
            return tok_from;
        }
        if (strval == "WHERE") {
            return tok_where;
        }
        if (strval == "AND") {
            return tok_operator;
        }
        if (strval == "OR") {
            return tok_operator;
        }
        // generic identifier (i.e. column name or table name)
        return tok_identifier;
    }
    if (isdigit(query[index]) || query[index] == '.') {
        // if the first character is a digit we simply have a numeric value
        strval = query[index];
        index++;
        while(isdigit(query[index]) || query[index] == '.')  {
            strval += query[index];
            index++;
        }
        numval = strtod(strval.c_str(), 0);
        return tok_constant;
    }
    if (IsOperator(query[index])) {
        // if the first character is an operator we consume the entire operator
        strval = query[index++];
        while(IsOperator(query[index])) {
            strval += query[index++];
        }
        // if the result is not a valid operator (e.g. >>>>) then return tok_invalid, otherwise return tok_operator
        if (IsOperator(strval)) {
            return tok_operator;
        }
    }
    // remaining special tokens
    if (query[index] == '(') {
        index++;
        return tok_leftparen;
    }
    if (query[index] == ')') {
        index++;
        return tok_rightparen;
    }
    if (query[index] == ',') {
        index++;
        return tok_comma;
    }
    index++;
    return tok_invalid;
}

static Token PeekToken(std::string query, size_t index) {
    // peek does not increment the index passed along
    return _ParseToken(query, index);
}

static Token ParseToken(std::string query, size_t& index) {
    // parse token increments the index, and moves forward in parsing
    Token token = _ParseToken(query, index);
    return token;
}

static Operation *ParseOperation(std::string query, size_t& index);
static Operation *ParsePrimary(std::string query, size_t& index) {
    // parse primary token, this can be either a constant value, identifier or the start of an expression (left parenthesis)
    Token token = ParseToken(query, index);
    switch(token) {
        case tok_constant:
            return new ConstantOperation(numval);
        case tok_identifier:
            return new ColumnOperation(std::string(strval));
        case tok_leftparen:
        {
            Operation *op = ParseOperation(query, index);
            if (ParseToken(query, index) != tok_rightparen) {
                std::cout << "Expected right parenthesis" << std::endl;
                return NULL;
            }
            return op;
        }
        default:
            std::cout << "Unexpected token " << TokToString(token) << std::endl;
            return NULL;
    }
}

static Operation *ParseRHS(std::string query, size_t &index, int precedence, Operation *LHS) {
    while(true) {
        // if the next token is an operator we parse the RHS of that operator
        Token token = PeekToken(query, index);
        if (token != tok_operator) {
            return LHS;
        }

        // if the operator has a lower precedence then the precedence of the current operator we stop
        std::string op = std::string(strval);
        int current_precedence = OperatorPrecedence(strval);
        if (current_precedence < precedence) {
            return LHS;
        }
        ParseToken(query, index);

        // now we parse the RHS of the operator
        Operation *RHS = ParsePrimary(query, index);
        if (!RHS) return NULL;

        // after parsing the RHS, we check if the next token is again an operator
        // if it is an operator and it has a higher precedence than the current operator than we have to evaluate that operator first
        // so we recursively call this function to obtain the RHS of this operator
        // we pass along the precedence of the current operator to the function
        // if the recursive RHS parse encounters an operator that has the same or lower precedence than us
        // we are evaluated first, since we came first in the function
        token = PeekToken(query, index);
        if (token == tok_operator && OperatorPrecedence(strval) > current_precedence) {
            RHS = ParseRHS(query, index, current_precedence + 1, RHS);
            if (!RHS) return NULL;
        }

        LHS = new BinaryOperation(LHS, RHS, op);
    }
}

static Operation *ParseOperation(std::string query, size_t& index) {
    // the first element of an operation is always a primary element (identifier, constant or left parenthesis)
    Operation *LHS = ParsePrimary(query, index);
    if (!LHS) return NULL;

    // now parse the right-hand side of the expression
    return ParseRHS(query, index, 0, LHS);
}

static OperationList* 
ParseOperationList(std::string query, size_t& index) {
    OperationList *base = (OperationList*) malloc(sizeof(OperationList));
    OperationList *collection = base, *prev = NULL;
    base->operation = NULL;
    base->next = NULL;
    while(true) {
        Operation *operation = ParseOperation(query, index);
        //PrintOperation(operation);
        if (prev != NULL) {
            collection = (OperationList*) malloc(sizeof(OperationList));
            prev->next = collection;
        }

        collection->operation = operation;
        collection->next = NULL;

        prev = collection;

        Token token = PeekToken(query, index);
        if (token == tok_comma) {
            ParseToken(query, index); //if there's a comma we have another expression to parse, consume the token
        }
        else {
            return base;
        }
    }
}

static Query *ParseQuery(std::string query) {
    // we only accept queries in the form SELECT [expr] FROM table WHERE [expr]
    Query *parsed_query = (Query*) malloc(sizeof(Query));
    Table *table;
    parsed_query->selection = NULL;
    parsed_query->table = NULL;
    parsed_query->where = NULL;
    size_t index = 0;
    Token token;
    char state = tok_invalid;
    while((token = ParseToken(query, index)) < tok_invalid) {
        switch(token) {
            case tok_select:
                // select is a collection of operations (separated by commas)
                if (state != tok_invalid) {
                    std::cout << "Unexpected SELECT." << std::endl;
                    return NULL;
                }
                state = tok_select;
                parsed_query->selection = ParseOperationList(query, index);
                if (parsed_query->selection == NULL) {
                    return NULL;
                }
                break;
            case tok_from:
                // from is just a table name, we don't support sub-queries here
                if (state != tok_select) {
                    std::cout << "Unexpected FROM." << std::endl;
                    return NULL;
                }
                state = tok_from;
                if ((token = ParseToken(query, index)) != tok_identifier) {
                    std::cout << "Expected table name after FROM." << std::endl;
                    return NULL;
                }
                parsed_query->table = strdup(strval.c_str());
                table = GetTable(parsed_query->table);
                if (table == NULL) {
                    std::cout << "Unrecognized table: " << parsed_query->table << std::endl;
                    return NULL;
                }
                break;
            case tok_where:
            {
                // where is a single operation
                if (state != tok_from) {
                    std::cout << "Unexpected WHERE." << std::endl;
                    return NULL;
                }
                state = tok_where;
                OperationList *collection = ParseOperationList(query, index);
                if (collection == NULL) {
                    return NULL;
                }
                if (collection->next != NULL) {
                    std::cout << "Unexpected comma in WHERE." << std::endl;
                    return NULL;
                }
                parsed_query->where = collection;
                break;
            }
            default:
                std::cout << "Unexpected token " << TokToString(token) << std::endl;
                return NULL;

        }
    }
    if (token == tok_invalid) {
        std::cout << "Failed to parse SQL query." << std::endl;
        free(parsed_query);
        return NULL;
    }
    for(OperationList *list = parsed_query->selection; list; list = list->next) {
        list->columns = GetColumns(table, list->operation);
        if (!list->columns) {
            return NULL;
        }
    }
    return parsed_query;
}

bool
_GetColumns(Table *table, Operation *op, ColumnList *current) {
    if (op->Type() == OPTYPE_binop) {
        return _GetColumns(table, ((BinaryOperation*)op)->LHS, current) && _GetColumns(table, ((BinaryOperation*)op)->RHS, current);
    } else if (op->Type() == OPTYPE_colmn) {
        Column *column = GetColumn(table, ((ColumnOperation*)op)->name);
        if (!column) {
            std::cout << "Unrecognized column name " << ((ColumnOperation*)op)->name << std::endl;
            return false; //unrecognized column
        }
        ((ColumnOperation*)op)->column = column;
        for(; current->next != NULL; current = current->next)  {
            if (current->column == column) return true;
        }
        if (current->column == column) return true;
        if (current->column != NULL) {
            current->next = (ColumnList*) malloc(sizeof(ColumnList));
            current = current->next;
            current->next = NULL;
        }
        current->column = column;
        return true;
    }
    // constant column
    return true;
}

static ColumnList*
GetColumns(Table *table, Operation *op) {
    ColumnList *list = (ColumnList*) malloc(sizeof(ColumnList));
    list->column = NULL;
    list->next = NULL;
    if (!_GetColumns(table, op, list)) return NULL; //unrecognized column
    return list;
}

static size_t
GetColCount(ColumnList *columns) {
    size_t count = 0;
    while(columns && columns->column) {
        columns = columns->next;
        count += 1;
    }
    return count;
}


#endif

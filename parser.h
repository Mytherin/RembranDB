
#include "assert.h"

#ifndef _PARSER_H_
#define _PARSER_H_

#define OPTYPE_binop 1
#define OPTYPE_colmn 2
#define OPTYPE_const 3

#define OPTYPE_mul 1    // multiplication: *
#define OPTYPE_div 2    // division: /
#define OPTYPE_add 3    // addition: +
#define OPTYPE_sub 4    // subtraction: -
#define OPTYPE_lt 5     // less than: <
#define OPTYPE_le 6     // less than or equal to: <=
#define OPTYPE_eq 7     // equal to: =
#define OPTYPE_ne 8     // not equal: !=
#define OPTYPE_gt 9     // greater than: >
#define OPTYPE_ge 10    // greater than or equal to: >=
#define OPTYPE_and 11   // and: &&
#define OPTYPE_or 12    // or: ||

static int OperatorType(const char* str);

#define Operation_BASE \
    int type;

typedef struct Operation {
    Operation_BASE
} Operation;

typedef struct {
    Operation_BASE
    double value;
} ConstantOperation;

typedef struct {
    Operation_BASE
    char *name;
    Column *column;
} ColumnOperation;

typedef struct {
    Operation_BASE
    const char *opname;
    int optype;
    Operation *left;
    Operation *right;
} BinaryOperation;

Operation *CreateConstantOperation(double val) {
    ConstantOperation *op = malloc(sizeof(ConstantOperation));
    op->value = val;
    op->type = OPTYPE_const;
    return (Operation*) op;
}

Operation *CreateColumnOperation(const char *name) {
    ColumnOperation *op = malloc(sizeof(ColumnOperation));
    op->name = strdup(name);
    op->type = OPTYPE_colmn;
    op->column = NULL;
    return (Operation*) op;
}

Operation *CreateBinaryOperation(const char *name, int optype, Operation *left, Operation *right) {
    BinaryOperation *op = malloc(sizeof(BinaryOperation));
    op->opname = strdup(name);
    op->optype = optype;
    op->left = left;
    op->right = right;
    op->type = OPTYPE_binop;
    return (Operation*) op;
}

struct _ColumnList;
typedef struct _ColumnList ColumnList;

struct _ColumnList {
    Column *column;
    ColumnList *next;
};
//scans the relevant columns from an operation
static ColumnList* GetColumns(Table *table, Operation *op); 
static ColumnList* UnionColumns(ColumnList *a, ColumnList *b);

struct _OperationList;
typedef struct _OperationList OperationList;

struct _OperationList {
    Operation *operation;
    ColumnList *columns; //relevant columns to the operation
    OperationList *next;
};

typedef struct {
    Operation *operation;
    ColumnList *columns; //relevant columns to the operation
} BaseOperation;

typedef struct {
    Operation *select;
    char *table;
    Operation *where;
    ColumnList *columns;
} Query;

typedef enum {
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
} Token;

static char* strval = NULL;
static double numval;

static bool IsOperatorCharacter(char c) {
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

static const char* TokToString(int token) {
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

static int OperatorType(const char *op) {
    // same operator precedence as C, higher = evaluated first
    // we separate operators by 100 so there is some room in between
    if (strcmp(op, "/") == 0) return OPTYPE_div;
    if (strcmp(op, "*") == 0) return OPTYPE_mul;
    if (strcmp(op, "+") == 0) return OPTYPE_add;
    if (strcmp(op, "-") == 0) return OPTYPE_sub;
    if (strcmp(op, ">=") == 0) return OPTYPE_ge;
    if (strcmp(op, ">") == 0) return OPTYPE_gt;
    if (strcmp(op, "<") == 0) return OPTYPE_lt;
    if (strcmp(op, "<=") == 0) return OPTYPE_le;
    if (strcmp(op, "==") == 0) return OPTYPE_eq;
    if (strcmp(op, "!=") == 0) return OPTYPE_ne;
    if (strcmp(op, "<>") == 0) return OPTYPE_ne;
    if (strcmp(op, "&&") == 0) return OPTYPE_and;
    if (strcmp(op, "AND") == 0) return OPTYPE_and;
    if (strcmp(op, "||") == 0) return OPTYPE_or;
    if (strcmp(op, "OR") == 0) return OPTYPE_or;
    return -1;
}

static int OperatorPrecedence(const char *op) {
    // same operator precedence as C, higher = evaluated first
    // we separate operators by 100 so there is some room in between
    if (strcmp(op, "/") == 0) return 1200;
    if (strcmp(op, "*") == 0) return 1200;
    if (strcmp(op, "+") == 0) return 1100;
    if (strcmp(op, "-") == 0) return 1100;
    if (strcmp(op, ">=") == 0) return 700;
    if (strcmp(op, ">") == 0) return 700;
    if (strcmp(op, "<") == 0) return 700;
    if (strcmp(op, "<=") == 0) return 700;
    if (strcmp(op, "==") == 0) return 600;
    if (strcmp(op, "!=") == 0) return 600;
    if (strcmp(op, "<>") == 0) return 600;
    if (strcmp(op, "&&") == 0) return 400;
    if (strcmp(op, "AND") == 0) return 400;
    if (strcmp(op, "||") == 0) return 300;
    if (strcmp(op, "OR") == 0) return 300;
    return -1;
}

static bool IsOperator(const char* str) {
    return OperatorPrecedence(str) > 0;
}

static Token _ParseToken(char *query, size_t *index) {
    // parse a token, incrementing the current index in the string as we go
    while(isspace(query[*index])) { //ignore all spaces
        (*index)++;
    }

    // ; or eof signals the end of the query
    if (query[*index] == '\0' || query[*index] == ';') return tok_eof;

    if (isalpha(query[*index])) { //identifiers must start with an alphabetic character (can't start with numbers)
        size_t str_start = *index;
        // scan until the current character is no longer a alphabetic character or number
        while(isalnum(query[*index]))  {
            (*index)++;
        }
        create_substring(&strval, query, str_start, *index);

        // handle special strings
        if (strcmp(strval, "SELECT") == 0) {
            return tok_select;
        }
        if (strcmp(strval, "FROM") == 0) {
            return tok_from;
        }
        if (strcmp(strval, "WHERE") == 0) {
            return tok_where;
        }
        if (strcmp(strval, "AND") == 0) {
            return tok_operator;
        }
        if (strcmp(strval, "OR") == 0) {
            return tok_operator;
        }
        // generic identifier (i.e. column name or table name)
        return tok_identifier;
    }
    if (isdigit(query[*index]) || query[*index] == '.') {
        // if the first character is a digit we simply have a numeric value
        size_t str_start = *index;
        (*index)++;
        while(isdigit(query[*index]) || query[*index] == '.')  {;
            (*index)++;
        }
        create_substring(&strval, query, str_start, *index);
        numval = strtod(strval, 0);
        return tok_constant;
    }
    if (IsOperatorCharacter(query[*index])) {
        // if the first character is an operator we consume the entire operator
        size_t str_start = *index;
        while(IsOperatorCharacter(query[*index])) {
            (*index)++;
        }
        create_substring(&strval, query, str_start, *index);
        // if the result is not a valid operator (e.g. >>>>) then return tok_invalid, otherwise return tok_operator
        if (IsOperator(strval)) {
            return tok_operator;
        }
    }
    // remaining special tokens
    if (query[*index] == '(') {
        (*index)++;
        return tok_leftparen;
    }
    if (query[*index] == ')') {
        (*index)++;
        return tok_rightparen;
    }
    if (query[*index] == ',') {
        (*index)++;
        return tok_comma;
    }
    (*index)++;
    return tok_invalid;
}

static Token PeekToken(char *query, size_t *index) {
    size_t copy_index = *index;
    // peek does not increment the index passed along
    return _ParseToken(query, &copy_index);
}

static Token ParseToken(char *query, size_t *index) {
    // parse token increments the index, and moves forward in parsing
    Token token = _ParseToken(query, index);
    return token;
}

static Operation *ParseOperation(char *query, size_t *index);
static Operation *ParsePrimary(char *query, size_t *index) {
    // parse primary token, this can be either a constant value, identifier or the start of an expression (left parenthesis)
    Token token = ParseToken(query, index);
    switch(token) {
        case tok_constant:
            return CreateConstantOperation(numval);
        case tok_identifier:
            return CreateColumnOperation(strval);
        case tok_leftparen:
        {
            Operation *op = ParseOperation(query, index);
            if (ParseToken(query, index) != tok_rightparen) {
                fprintf(stderr, "Expected right parenthesis.\n");
                return NULL;
            }
            return op;
        }
        default:
            fprintf(stderr, "Unexpected token %s.\n", TokToString(token));
            return NULL;
    }
}

static Operation *ParseRHS(char *query, size_t *index, int precedence, Operation *LHS) {
    while(true) {
        // if the next token is an operator we parse the RHS of that operator
        Token token = PeekToken(query, index);
        if (token != tok_operator) {
            return LHS;
        }

        // if the operator has a lower precedence then the precedence of the current operator we stop
        char *op = strdup(strval);
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

        LHS = CreateBinaryOperation(op, OperatorType(op), LHS, RHS);
    }
}

static Operation *ParseOperation(char *query, size_t *index) {
    // the first element of an operation is always a primary element (identifier, constant or left parenthesis)
    Operation *LHS = ParsePrimary(query, index);
    if (!LHS) return NULL;

    // now parse the right-hand side of the expression
    return ParseRHS(query, index, 0, LHS);
}

static OperationList* 
ParseOperationList(char *query, size_t *index) {
    OperationList *base = (OperationList*) malloc(sizeof(OperationList));
    OperationList *collection = base, *prev = NULL;
    base->operation = NULL;
    base->next = NULL;
    while(true) {
        Operation *operation = ParseOperation(query, index);
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

// Inverts a linked-list of columns
// Returns the first element of the inverted list.
static Column*
InvertColumnList(Column *list) {
    Column *result = NULL;
    while(list) {
        Column *next = list->next;
        list->next = result;
        result = list;
        list = next;
    }
    return result;
}

// Inverts a linked-list of operations. 
// Returns the first element of the inverted list.
static OperationList*
InvertList(OperationList *list) {
    OperationList *result = NULL;
    while(list) {
        OperationList *next = list->next;
        list->next = result;
        result = list;
        list = next;
    }
    return result;
}

static OperationList*
SelectStarFromTable(Table *table) {
    OperationList *collection = NULL;
    Column *columns = table->columns;
    while(columns) {
        OperationList *op = (OperationList*) malloc(sizeof(OperationList));
        op->next = collection;
        op->operation = CreateColumnOperation(columns->name);
        collection = op;
        columns = columns->next;
    }
    return InvertList(collection);
}

static Query *ParseQuery(char* query) {
    // we only accept queries in the form SELECT [expr] FROM table WHERE [expr]
    Query *parsed_query = (Query*) malloc(sizeof(Query));
    Table *table;
    parsed_query->select = NULL;
    parsed_query->table = NULL;
    parsed_query->where = NULL;
    bool select_all = false;
    size_t index = 0;
    Token token;
    char state = tok_invalid;
    while((token = ParseToken(query, &index)) < tok_invalid) {
        switch(token) {
            case tok_select:
            {
                // select is a collection of operations (separated by commas)
                if (state != tok_invalid) {
                    fprintf(stderr, "Unexpected SELECT.\n");
                    return NULL;
                }
                state = tok_select;
                Token peek = PeekToken(query, &index);
                // handle "SELECT * FROM table" as special case
                if (peek == tok_operator && strval[0] == '*') {
                    // consume the token
                    ParseToken(query, &index);
                    select_all = true;
                } else {
                    parsed_query->select = ParseOperationList(query, &index)->operation;
                    if (parsed_query->select == NULL) {
                        return NULL;
                    }
                }
                break;
            }
            case tok_from:
                // from is just a table name, we don't support sub-queries here
                if (state != tok_select) {
                    fprintf(stderr, "Unexpected FROM.\n");
                    return NULL;
                }
                state = tok_from;
                if ((token = ParseToken(query, &index)) != tok_identifier) {
                    fprintf(stderr, "Expected table name after FROM.");
                    return NULL;
                }
                parsed_query->table = strdup(strval);
                table = GetTable(parsed_query->table);
                if (table == NULL) {
                    fprintf(stderr, "Unrecognized table: %s\n", parsed_query->table);
                    return NULL;
                }
                break;
            case tok_where:
            {
                if (state != tok_from) {
                    fprintf(stderr, "Unexpected WHERE.\n");
                    return NULL;
                }
                state = tok_where;
                OperationList *collection = ParseOperationList(query, &index);
                if (collection == NULL) {
                    return NULL;
                }
                // where contains a single operation (x && y && z)
                // unlike SELECT clause which can have multiple operations (x, y, z)
                if (collection->next != NULL) {
                    fprintf(stderr, "Unexpected comma in WHERE.\n");
                    return NULL;
                }
                parsed_query->where = collection->operation;
                break;
            }
            default:
                fprintf(stderr, "Unexpected token %s\n", TokToString(token));
                return NULL;

        }
    }
    if (token == tok_invalid) {
        fprintf(stderr, "Failed to parse SQL query.\n");
        free(parsed_query);
        return NULL;
    }
    if (select_all) {
        // get all table columns
        parsed_query->select = SelectStarFromTable(GetTable(parsed_query->table))->operation;
    }
    ColumnList *select_columns = GetColumns(table, parsed_query->select);
    if (select_columns == NULL) {
        return NULL;
    }
    ColumnList *where_columns = NULL;
    if (parsed_query->where != NULL) {
        where_columns = GetColumns(table, parsed_query->where);
        if (where_columns == NULL) {
            return NULL;
        }
    }
    parsed_query->columns = UnionColumns(select_columns, where_columns);
    return parsed_query;
}

bool
_GetColumns(Table *table, Operation *op, ColumnList *current) {
    if (!op) return true;
    if (op->type== OPTYPE_binop) {
        return _GetColumns(table, ((BinaryOperation*)op)->left, current) && _GetColumns(table, ((BinaryOperation*)op)->right, current);
    } else if (op->type == OPTYPE_colmn) {
        Column *column = GetColumn(table, ((ColumnOperation*)op)->name);
        if (!column) {
            fprintf(stderr, "Unrecognized column name %s\n", ((ColumnOperation*)op)->name);
            return false; //unrecognized column
        }
        // this column is used in a query, read the column data into memory if it is not already there
        ReadColumnData(column);
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

static 
bool ColumnInList(ColumnList *l, Column *c) {
    while (l) {
        if (l->column == c) {
            return true;
        }
        l = l->next;
    }
    return false;
}

static ColumnList* Tail(ColumnList *l) {
    while(l->next) {
        l = l->next;
    }
    return l;
}

static ColumnList*
UnionColumns(ColumnList *a, ColumnList *b) {
    if (a == NULL && b == NULL) assert(0);
    if (a == NULL) return b;
    if (b == NULL) return a;
    ColumnList *tail = Tail(a);
    while(b) {
        if (!ColumnInList(a, b->column)) {
            tail->next = (ColumnList*) malloc(sizeof(ColumnList));
            tail->next->column = b->column;
            tail->next->next = NULL;
            tail = tail->next;
        }
        b = b->next;
    }
    return a;
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

void _unused_() {
    (void) GetLLVMType;
    (void) InvertColumnList;
    (void) GetColCount;
}


#endif




#ifndef _TABLE_H_
#define _TABLE_H_

#include <string.h>

#define TYPE_int 1
#define TYPE_lng 2
#define TYPE_flt 3
#define TYPE_dbl 4
#define TYPE_str 5

typedef long long lng;
typedef float flt;
typedef double dbl;

unsigned char elsize[] = { 4, 8, 4, 8 };

struct _Column;
typedef struct _Column Column;
struct _Column {
    char *name;
    unsigned char type;
    unsigned char elsize;
    lng base_oid;
    lng size;
    void *data;
    Column *next;
    char *data_location;
    LLVMValueRef llvm_ptr;
};

typedef struct {
    char *name;
    Column *columns;
} Table;

#define MAX_TABLES 100

static Table *tables[MAX_TABLES];
static int current_table = 0;

static Column *GetColumn(Table *table, char *name) {
    Column *column;
    for(column = table->columns; column; column = column->next) {
        if (strcmp(column->name, name) == 0) {
            return column;
        }
    }
    return NULL;
}

static Table *GetTable(const char *name) {
    for(int i = 0; i < current_table; i++) {
        if (tables[i] && strcmp(tables[i]->name, name) == 0) {
            return tables[i];
        }
    }
    return NULL;
}

static void RegisterTable(Table *table) {
    tables[current_table++] = table;
}

static void create_substring(char **res, char *str, size_t start, size_t end) {
    *res = (char*) calloc((1 + end - start), sizeof(char));
    memcpy(*res, &str[start], end - start);
}

static char** split(char *string, char delimiter, size_t *splits) {
    size_t length = strlen(string);
    size_t i = 0;
    *splits = 1;
    for(i = 0; i < length; i++) {
        if (string[i] == delimiter) {
            (*splits)++;
        }
    }
    char **split_values = (char**) malloc(*splits * sizeof(char*));
    size_t start_index = 0, current_split = 0;
    for(i = 0; i < length + 1; i++) {
        if (i == length || string[i] == delimiter) {
            create_substring(&split_values[current_split], string, start_index, i);
            current_split++;
            start_index = i + 1;
        }
    }
    return split_values;
}

static void 
ReadColumnData(Column *column) {
    if (!column) return;
    if (column->data) return;

    FILE *fp = fopen(column->data_location, "r");
    column->data = malloc(column->size * column->elsize);
    if (column->data == NULL) {
        printf("Failed to allocate memory for column.\n");
        return;
    }
    size_t elements_read = fread(column->data, column->elsize, column->size, fp);
    if (elements_read != (size_t) (column->size)) {
        printf("Read incorrect number of elements from file %s, expected %lld elements but read %zu elements.\n", column->data_location, column->size, elements_read);
        return;
    }
    fclose(fp);
}

// Reads a Table from a CSV file (the CSV file must have header information + type information included)
static Table* ReadTable(const char *table_name, char *name) {
    FILE *fp = fopen(name, "r");
    if (fp == NULL ) {
        printf("Failed to open file %s.\n", name);
        return NULL;
    }
    Table *table = (Table*) malloc(sizeof(Table));
    table->name = strdup(table_name);
    table->columns = NULL;

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        size_t split_count;
        char **splits = split(line, ' ', &split_count);
        if (split_count < 3) {
            printf("Expected [name] [type] [size].\n");
            return NULL;
        }
        Column *column = (Column*) malloc(sizeof(Column));
        column->name = strdup(splits[0]);
        column->next = table->columns;

        if (strcmp(splits[1], "int") == 0) {
            column->type = TYPE_int;
            column->elsize = 4;
        } else if (strcmp(splits[1], "lng") == 0) {
            column->type = TYPE_lng;
            column->elsize = 8;
        }  else if (strcmp(splits[1], "flt") == 0) {
            column->type = TYPE_flt;
            column->elsize = 4;
        }  else if (strcmp(splits[1], "dbl") == 0) {
            column->type = TYPE_dbl;
            column->elsize = 8;
        }
        column->base_oid = 0;
        column->size = atoll(splits[2]);
        char column_file_name[500];
        snprintf(column_file_name, 500, "Tables/%s/%s.col", table->name, column->name);
        FILE *fp = fopen(column_file_name, "r");
        if (fp == NULL) {
            printf("Unable to open file %s\n", column_file_name);
            return NULL;
        }
        fclose(fp);
        column->data = NULL;
        column->data_location = strdup(column_file_name);
        table->columns = column;
        ReadColumnData(column);
    }
    return table;
}


static void InitializeTable(const char *name) {
    char table_file[500];
    snprintf(table_file, 500, "Tables/%s.tbl", name); 
    printf("# Load table %s from file Tables/%s.tbl.\n", name, name);
    RegisterTable(ReadTable(name, table_file));
}


static void PrintPaddedString(const char *str, size_t width, char padding) {
    size_t length = strlen(str);
    if (length >= width) {
        printf("%s", str);
        return;
    }
    size_t pad_size = (width - length) / 2;
    for(size_t i = 0; i < pad_size; i++) {
        printf("%c", padding);
    }
    printf("%s", str);
    for(size_t i = 0; i < pad_size; i++) {
        printf("%c", padding);
    }
    if ((length + 2 * pad_size) != width) {
        printf("%c", padding);
    }
}

static size_t GetWidthType(int type) {
    switch(type) {
        case TYPE_int:
            return 10;
        case TYPE_lng:
            return 20;
        case TYPE_flt:
            return 20;
        case TYPE_dbl:
            return 30;
        case TYPE_str:
            return 20;
    }
    return -1;
}

static size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

static size_t GetWidth(Column *col) {
    return max(GetWidthType(col->type), strlen(col->name));
}

static void PrintTables() {
    size_t width = 0;
    for(int i = 0; i < current_table; i++) {
        if (tables[i]) {
            width = max(width, strlen(tables[i]->name) + 4);
        }
    }
    PrintPaddedString("", width, '-'); printf("\n");
    PrintPaddedString("Tables", width, '-'); printf("\n");
    PrintPaddedString("", width, '-'); printf("\n");

    for(int i = 0; i < current_table; i++) {
        if (tables[i]) {
            printf("|");
            PrintPaddedString(tables[i]->name, width - 2, ' ');
            printf("|\n");
        }
    }
    PrintPaddedString("", width, '-');
    printf("\n");
}

static void PrintTable(Table *table) {
    if (!table) return;
    size_t width = 0;
    Column *current = table->columns;
    while (current != NULL) {
        width += GetWidth(current) + 2;
        current = current->next;
    }

    PrintPaddedString("", width, '-'); printf("\n");
    PrintPaddedString(table->name, width, '-'); printf("\n");
    PrintPaddedString("", width, '-'); printf("\n");

    current = table->columns;
    while (current != NULL) {
        size_t curwidth = GetWidth(current);
        printf("|");
        PrintPaddedString(current->name, curwidth, ' ');
        printf("|");
        current = current->next;
    }

    printf("\n");
    PrintPaddedString("", width, '-'); printf("\n");

    lng index = 0;
    while(index < table->columns->size && index < 50) {
        current = table->columns;
        while (current != NULL) {
            size_t curwidth = GetWidth(current);
            printf("|");
            char value[500];
            switch(current->type) {
                case TYPE_int:
                    snprintf(value, 500, "%d", ((int*)current->data)[index]);
                    break;
                case TYPE_lng:
                    snprintf(value, 500, "%lld", ((lng*)current->data)[index]);
                    break;
                case TYPE_flt:
                    snprintf(value, 500, "%f", ((flt*)current->data)[index]);
                    break;
                case TYPE_dbl:
                    snprintf(value, 500, "%lf", ((dbl*)current->data)[index]);
                    break;
            }
            PrintPaddedString(value, curwidth, ' ');
            printf("|");
            current = current->next;
        }
        printf("\n");
        index++;
    }
    PrintPaddedString("", width, '-'); printf("\n");

    if (index < table->columns->size) {
        printf("An additional %lld columns were not printed (total results: %lld).\n", table->columns->size - index, table->columns->size);
    }
}

static LLVMTypeRef
GetLLVMType(int type) {
    switch(type) {
        case TYPE_int:
            return LLVMInt32Type();
        case TYPE_lng:
            return LLVMInt64Type();
        case TYPE_flt:
            return LLVMFloatType();
        case TYPE_dbl:
            return LLVMDoubleType();
    }
    return NULL;
}

#endif

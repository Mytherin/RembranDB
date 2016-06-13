


#ifndef _TABLE_HPP_
#define _TABLE_HPP_

#include <string>
#include <iostream>
#include <fstream>
#include <cstring>

#define TYPE_int 1
#define TYPE_lng 2
#define TYPE_flt 3
#define TYPE_dbl 4
#define TYPE_str 5

typedef long long lng;
typedef float flt;
typedef double dbl;

unsigned char elsize[] = { 4, 8, 4, 8 };

struct Column {
    char *name;
    unsigned char type;
    unsigned char elsize;
    lng base_oid;
    lng size;
    void *data;
    Column *next;
    char *data_location = NULL;
    llvm::AllocaInst *address;
};

struct Table {
    char *name;
    Column *columns;
};

#define MAX_TABLES 100

static Table *tables[MAX_TABLES];
static int current_table = 0;

static Column *GetColumn(Table *table, std::string name) {
    Column *column;
    for(column = table->columns; column; column = column->next) {
        if (column->name == name) {
            return column;
        }
    }
    return NULL;
}

static Table *GetTable(std::string name) {
    for(int i = 0; i < current_table; i++) {
        if (tables[i] && tables[i]->name == name) {
            return tables[i];
        }
    }
    return NULL;
}

static void RegisterTable(Table *table) {
    tables[current_table++] = table;
}

static std::vector<std::string> split(const std::string& str, int delimiter(int) = ::isspace){
    std::vector<std::string> result;
    auto e = str.end();
    auto i = str.begin();
    while(i != e) {
        i = find_if_not(i, e, delimiter);
        if (i == e) break;
        auto j = find_if(i, e, delimiter);
        result.push_back(std::string(i, j));
        i = j;
    }
    return result;
}

// Reads a Table from a CSV file (the CSV file must have header information + type information included)
static Table* ReadTable(std::string table_name, std::string name) {
    std::string line;
    std::ifstream csv(name);
    if (csv.is_open()) {
        Table *table = (Table*) malloc(sizeof(Table));
        table->name = strdup(table_name.c_str());
        table->columns = NULL;
        while(getline(csv, line)) {
            auto splits = split(line);
            Column *column = (Column*) malloc(sizeof(Column));
            column->name = strdup(splits[0].c_str());
            column->next = table->columns;
            if (splits[1].compare("int") == 0) {
                column->type = TYPE_int;
                column->elsize = 4;
            } else if (splits[1].compare("lng") == 0) {
                column->type = TYPE_lng;
                column->elsize = 8;
            }  else if (splits[1].compare("flt") == 0) {
                column->type = TYPE_flt;
                column->elsize = 4;
            }  else if (splits[1].compare("dbl") == 0) {
                column->type = TYPE_dbl;
                column->elsize = 8;
            }
            column->base_oid = 0;
            column->size = std::stoll(splits[2]);
            std::string colfname = std::string("Tables/") + table_name + std::string("/") + splits[0] + std::string(".col");
            FILE *fp = fopen(colfname.c_str(), "r");
            if (fp == NULL) {
                std::cout << "Unable to open file " << colfname << std::endl;
                return NULL;
            }
            fclose(fp);
            column->data = NULL;
            column->data_location = strdup(colfname.c_str());
            table->columns = column;
        }
        return table;
    } else {
        std::cout << "Unable to open file " << name << std::endl;
        return NULL;
    }
}


static void ReadTables(std::vector<std::string> names) {
    for(auto it = names.begin(); it != names.end(); it++) {
        RegisterTable(ReadTable(*it, std::string("Tables/") + *it + std::string(".tbl")));
    }
}


static std::string PadString(std::string str, size_t width, char padding) {
    if (str.size() >= width) {
        return str;
    }
    size_t pad_size = (width - str.size()) / 2;
    str.insert(str.begin(), pad_size, padding);
    str.insert(str.end(), pad_size, padding);
    if (str.size() != width) {
        str.insert(str.end(), 1, padding);
    }
    return str;
}

static size_t GetWidth(int type) {
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

static size_t GetWidth(Column *col) {
    return fmax(GetWidth(col->type), strlen(col->name));
}

static void PrintTables() {
    size_t width = 0;
    for(int i = 0; i < current_table; i++) {
        if (tables[i]) {
            width = fmax(width, strlen(tables[i]->name) + 4);
        }
    }
    std::cout << PadString("", width, '-') << std::endl;
    std::cout << PadString(std::string("Tables"), width, '-') << std::endl;
    std::cout << PadString("", width, '-') << std::endl;

    for(int i = 0; i < current_table; i++) {
        if (tables[i]) {
            std::cout << "|" << PadString(tables[i]->name, width - 2, ' ') << "|" << std::endl;
        }
    }
    std::cout << PadString("", width, '-') << std::endl;
}

static void PrintTable(Table *table) {
    if (!table) return;
    size_t width = 0;
    Column *current = table->columns;
    while (current != NULL) {
        width += GetWidth(current) + 2;
        current = current->next;
    }

    std::cout << PadString("", width, '-') << std::endl;
    std::cout << PadString(table->name, width, '-') << std::endl;
    std::cout << PadString("", width, '-') << std::endl;


    current = table->columns;
    while (current != NULL) {
        size_t curwidth = GetWidth(current);
        std::cout << "|" << PadString(current->name, curwidth, ' ') << "|";
        current = current->next;
    }

    std::cout << std::endl;
    std::cout << PadString("", width, '-') << std::endl;

    lng index = 0;
    while(index < table->columns->size && index < 50) {
        current = table->columns;
        while (current != NULL) {
            size_t curwidth = GetWidth(current);
            std::cout << "|";
            switch(current->type) {
                case TYPE_int:
                    std::cout << PadString(std::to_string(((int*)current->data)[index]), curwidth, ' ');
                    break;
                case TYPE_lng:
                    std::cout << PadString(std::to_string(((lng*)current->data)[index]), curwidth, ' ');
                    break;
                case TYPE_flt:
                    std::cout << PadString(std::to_string(((flt*)current->data)[index]), curwidth, ' ');
                    break;
                case TYPE_dbl:
                    std::cout << PadString(std::to_string(((dbl*)current->data)[index]), curwidth, ' ');
                    break;
            }
            std::cout << "|";
            current = current->next;
        }
        std::cout << std::endl;
        index++;
    }
    std::cout << PadString("", width, '-') << std::endl;

    if (index < table->columns->size) {
        std::cout << "An additional " << table->columns->size - index << " columns  were not printed (total results: " << table->columns->size << ")." << std::endl;
    }
}

static void 
ReadColumnData(Column *column) {
    if (!column) return;
    if (column->data) return;

    FILE *fp = fopen(column->data_location, "r");
    column->data = malloc(column->size * column->elsize);
    if (column->data == NULL) {
        std::cout << "Malloc fail" << std::endl;
        return;
    }
    size_t elements_read = fread(column->data, column->elsize, column->size, fp);
    if (elements_read != (size_t) (column->size)) {
        std::cout << "Read incorrect number of elements from file " << column->data_location << ", expected " << column->size << " but read " << elements_read << std::endl;
        return;
    }
    fclose(fp);
}

static llvm::Type*
GetLLVMType(llvm::LLVMContext& context, int type) {
    switch(type) {
        case TYPE_int:
            return llvm::Type::getInt32Ty(context);
        case TYPE_lng:
            return llvm::Type::getInt64Ty(context);
        case TYPE_flt:
            return llvm::Type::getFloatTy(context);
        case TYPE_dbl:
            return llvm::Type::getDoubleTy(context);
    }
    return NULL;
}

#endif

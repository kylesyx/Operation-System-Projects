#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <iomanip>
#include <string.h>

using namespace std;

#define BUFSZ 1024

static char* filename;
static char IAER[] = "IAER";
static int numTok=0;
static int processedTok=0;
static int numSym=0;
static int x=0;
static int y=0;
static int lastx=0;
static int lasty=0;


struct token {
    char value[100] = "";//if more than 16, error
    int lineNum = 0;
    int offset = 0;
};

struct symbol{
    char value[100]="";
    int address = -1;
    int mod_num = -1;
    bool used = false;
    char* error=NULL;
    bool defined = true;
};

struct module{
    int mod_num = -1;
    int address = -1;
};

token getToken(ifstream& infile)
{
    static bool newline = 1;
    static char buffer[BUFSZ];
    token tok;

    char* temp=NULL;
    while (1) {
        while (newline) {

            x++;
            y=1;
            infile.getline(buffer, BUFSZ);
            if (infile.eof()) {
                //cout<<"Final Spot in File: line="<<lastx<<" offset="<<lasty<<endl;
                return tok;
            }
            int lastindex=buffer[strlen(buffer)-1];
            //cout<<lastindex<<endl;
            if(strlen(buffer)==0){
                //cout<<"end with space"<<endl;
                lastx=x;
                lasty=1;
            }
            else if (lastindex!=10){
                //cout<<"end with other"<<lastx<<endl;
                lastx=x;
                lasty=strlen(buffer)+1;
                //cout<<buffer<<" "<<strlen(buffer)<<endl;
            }
            temp = strtok(buffer, " \t");
            if (temp != NULL) {
                strcpy(tok.value, temp);
                if ((strcmp(tok.value, "") != 0)) {
                    newline = 0;
                    y=temp-buffer+1;
                    tok.lineNum = x;
                    //lastx = x;
                    tok.offset = y;
                    //lasty=y+strlen(tok.value);
                    return tok;
                }
            } else {
                break;
            }
        }
        temp = strtok(NULL, " \t");
        if (temp!=NULL) {
            strcpy(tok.value, temp);
            y=temp-buffer+1;
            tok.lineNum=x;
            //lastx=x;
            tok.offset=y;
            //lasty=y+strlen(tok.value);
            return tok;
        }
        else {
            newline = 1;
            continue;
        }
    }
}

token* tokenizer(char* filename) {
    ifstream infile(filename);
    int pos = 0;
    token tokens[512];
    token temp;
    if (!infile.is_open()) {
        printf("Error: Could not open file\n");
        exit(1);
    }
    while (1) {
        temp = getToken(infile);
        if ((strcmp(temp.value, "") == 0)) {
            //cout<<"file End"<<endl;
            infile.close();
            return tokens;
        }
        tokens[pos] = temp;
        numTok++;
        //cout <<"Token: "<<tokens[pos].lineNum<<": "<<tokens[pos].offset<<" : "<<tokens[pos].value << endl;
        pos++;
    }
}

void parseerror(int errorcode, token tok){
    static char* errstr[]={
            "NUM_EXPECTED",
            "SYM_EXPECTED",
            "ADDR_EXPECTED",
            "SYM_TOO_LONG",
            "TOO_MANY_DEF_IN_MODULE",
            "TOO_MANY_USE_IN_MODULE",
            "TOO_MANY_INSTR",
    };
    //cout<<tok.value<<endl;
    printf("Parse Error line %d offset %d: %s\n", tok.lineNum, tok.offset, errstr[errorcode]);
    exit(1);
}

token readInt(token* tok){
    if (strlen(tok[processedTok].value)==0){
        tok[processedTok].lineNum=lastx;
        tok[processedTok].offset=lasty;
        parseerror(0, tok[processedTok]);
    }
    for (int a=0;a<strlen(tok[processedTok].value);a++){
        if (!isdigit(tok[processedTok].value[a])) parseerror(0, tok[processedTok]);
    }
    processedTok++;
    return tok[processedTok-1];
}

token readSymbol(token* tok){
    if (strlen(tok[processedTok].value)==0) {
        tok[processedTok].lineNum=lastx;
        tok[processedTok].offset=lasty;
        parseerror(1, tok[processedTok]);
    }
    if (strlen(tok[processedTok].value)>16) parseerror(3, tok[processedTok]);
    if (!isalpha(tok[processedTok].value[0])) parseerror(1, tok[processedTok]);
    for (int a=0;a<strlen(tok[processedTok].value);a++){
        if (!isalnum(tok[processedTok].value[a])) parseerror(1, tok[processedTok]);
    }
    processedTok++;
    return tok[processedTok-1];
}

token readIAER(token* tok){
    if (strlen(tok[processedTok].value)==0) {
        tok[processedTok].lineNum=lastx;
        tok[processedTok].offset=lasty;
        parseerror(2, tok[processedTok]);
    }
    if ((strlen(tok[processedTok].value)!=1 )|| (strcmp(tok[processedTok].value,"A")&&strcmp(tok[processedTok].value,"E")&&strcmp(tok[processedTok].value,"I")&&strcmp(tok[processedTok].value,"R"))) {
        parseerror(2, tok[processedTok]);
    }
    processedTok++;
    return tok[processedTok-1];
}

void rule4(symbol* syms){
    for (int i=0; i<numSym; i++){
        if (!syms[i].used){
            printf("\nWarning: Module %d: %s was defined but never used", syms[i].mod_num, syms[i].value);
        }
    }
    printf("\n");
}

void rule5(symbol* syms, int upos){
    for (int i=0; i<upos; i++){
        if (syms[i].defined&&!syms[i].used){
            printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", syms[i].mod_num, syms[i].value);
        }
    }
}

symbol* pass1(char* filename){
    token* tokens=tokenizer(filename);
    module modules[512];
    symbol symbols[512];
    int mpos=0;
    int spos=0;
    int base_addr=0;
    int numInstr=0;

    while (processedTok<numTok) {
        bool next=false;
        module mod;
        int defcount = atoi(readInt(tokens).value);
        if (defcount>16) parseerror(4, tokens[processedTok-1]);

        for (int i = 0; i < defcount; i++) {
            symbol sym;
            token temp = readSymbol(tokens);
            strcpy(sym.value,temp.value);
            sym.address = atoi(readInt(tokens).value)+base_addr;
            if (spos!=0){
                for (int i=0; i<spos; i++){
                    if (!strcmp(sym.value, symbols[i].value)){
                        symbols[i].error="Error: This variable is multiple times defined; first value used";
                        next=true;
                        break;//找到相同symbol 跳入下一个symbol添加
                    }
                }
            }
            //无相同 添加symbol
            if (next){
                next=false;
                continue;
            }

            sym.mod_num=mpos+1;
            symbols[spos]=sym;
            spos++;
        }

        int usecount = atoi(readInt(tokens).value);
        if (usecount>16) parseerror(5, tokens[processedTok-1]);

        for (int i = 0; i < usecount; i++) {
            readSymbol(tokens);
        }

        int instcount = atoi(readInt(tokens).value);
        numInstr+=instcount;
        if (numInstr>512) parseerror(6, tokens[processedTok-1]);
        for (int i = 0; i < instcount; i++) {
            readIAER(tokens);
            readInt(tokens);
        }
        mod.address=base_addr;
        for (int i=0; i<spos; i++){
            if(symbols[i].address>instcount+mod.address){
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", symbols[i].mod_num, symbols[i].value, symbols[i].address, instcount+mod.address-1);
                symbols[i].address=mod.address;
            }
        }
        base_addr+=instcount;
        mod.mod_num=mpos+1;
        modules[mpos]=mod;
        mpos++;
    }
    cout<<"Symbol Table"<<endl;
    numSym=spos;
    processedTok=0;
    numTok=0;
    x=0;
    y=0;
    lastx=0;
    lasty=0;
    return symbols;
}

void pass2(char* filename, symbol* symTable){
    symbol symList[numSym];
    for (int i=0;i<numSym;i++){
        symList[i]=symTable[i];
    }
    token* tokens=tokenizer(filename);
    module modules[512];
    int numInstr=-1;
    int mpos=0;
    int base_addr=0;
    bool next=false;
    cout<<"\nMemory Map"<<endl;
    while (processedTok<numTok) {
        module mod;
        symbol usedList[16];
        int upos=0;
        int defcount = atoi(readInt(tokens).value);
        for (int i = 0; i < defcount; i++) {
            readSymbol(tokens);
            readInt(tokens);
        }

        int usecount = atoi(readInt(tokens).value);
        for (int i = 0; i < usecount; i++) {

            symbol usedsym;
            token temp = readSymbol(tokens);
            strcpy(usedsym.value,temp.value);
            for (int i=0; i<numSym; i++) {
                if (!strcmp(usedsym.value, symList[i].value)) {
                    symList[i].used = true;
                }
            }
            usedsym.mod_num=mpos+1;
            usedList[upos]=usedsym;
            upos++;
        }

        int instcount = atoi(readInt(tokens).value);

        for (int i = 0; i < instcount; i++) {
            numInstr++;
            char addressmode = readIAER(tokens).value[0];
            int operand = atoi(readInt(tokens).value);
            if (addressmode=='A'){
                if (operand>=10000) {
                    operand = 9999;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Illegal opcode; treated as 9999"<<endl;
                    continue;
                }
                if (operand%1000>512){
                    operand=operand-operand%1000;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Absolute address exceeds machine size; zero used"<<endl;
                    continue;
                }
                cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<endl;
                continue;
            }
            if (addressmode=='E'){
                if (operand>=10000) {
                    operand = 9999;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Illegal opcode; treated as 9999"<<endl;
                    continue;
                }
                if (operand%1000>=upos){
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: External address exceeds length of uselist; treated as immediate"<<endl;
                    continue;
                }
                for (int i=0; i<numSym; i++){
                    if (!strcmp(usedList[operand%1000].value, symList[i].value)){
                        usedList[operand%1000].used=true;
                        operand=operand-operand%1000+symList[i].address;
                        cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<endl;
                        next=true;
                        break;
                    }
                }
                if (!next){
                    int ope=operand-operand%1000;
                    usedList[operand%1000].defined=false;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << ope<<" Error: "<<usedList[operand%1000].value<<" is not defined; zero used"<<endl;
                }
            }
            if (addressmode=='R'){
                if (operand>=10000) {
                    operand = 9999;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Illegal opcode; treated as 9999"<<endl;
                    continue;
                }
                if (operand%1000>instcount){
                    operand=operand-operand%1000;
                    operand+=base_addr;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Relative address exceeds module size; zero used"<<endl;
                    continue;
                }
                operand+=base_addr;
                cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<endl;
                continue;
            }
            if (addressmode=='I'){
                if (operand>=10000){
                    operand=9999;
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<" Error: Illegal immediate value; treated as 9999"<<endl;
                    continue;
                }
                else {
                    cout << setw(3) << setfill('0') << numInstr<<": "<<setw(4) << setfill('0') << operand<<endl;
                    continue;
                }
            }
        }
        mod.address=base_addr;
        base_addr+=instcount;
        mod.mod_num=mpos+1;
        modules[mpos]=mod;
        mpos++;
        rule5(usedList, upos);
    }
    rule4(symList);
}

int main(int argc, char* argv[]) {
    symbol* symbols;
    filename = argv[1];
    symbols= pass1(filename);
    for (int x=0;x<numSym;x++){
        if (symbols[x].error!=NULL){
            cout<<symbols[x].value<<"="<<symbols[x].address<<" "<<symbols[x].error<<endl;
        }
        else{
            cout<<symbols[x].value<<"="<<symbols[x].address<<endl;
        }
    }
    pass2(filename, symbols);
}



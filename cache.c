/* gcc -o teste cache.c -ansi -Wall -g */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

/*
\033[22;30m - black
\033[22;31m - red
\033[22;32m - green
\033[22;33m - brown
\033[22;34m - blue
\033[22;35m - magenta
\033[22;36m - cyan
\033[22;37m - gray
\033[01;30m - dark gray
\033[01;31m - light red
\033[01;32m - light green
\033[01;33m - yellow
\033[01;34m - light blue
\033[01;35m - light magenta
\033[01;36m - light cyan
\033[01;37m - white
*/

#define Color_Red "\033[01;31m"
#define Color_Blue "\033[01;34m"
#define Color_Cyan "\033[01;36m"
#define Color_end "\33[0m"

#define LINELENGTH 128

/* (Bytes) */
#define L1_SIZE 16384
#define L2_SIZE 128384
#define BLOCK_SIZE 4

#define DEBUG 0

#define TAG 18
#define INDEX 12
#define OFFSET 2

typedef struct Bloco_ {
    int out;
    int valid;
    char *tag;
} *Bloco;

typedef struct Cache_ {
    int hits;
    int misses;
    int reads;
    int writes;
    int tamanhoCache;
    int block_size;
    int numLinhas;
    int mapeamento;
    Bloco *blocos;
    struct Cache_ *nextLevel;
} *Cache;


Cache createCache(int tamanhoCache, int block_size, int mapeamento);

int buscarNaCache(Cache cache, char* address);

unsigned int HexaParaInteiro(const char str[]);

char *getBinary(unsigned int num);

char *formatBinary(char *bstring);

int binarioParaDecimal(char *bin);


int main(int argc, char **argv) {
    setlocale(LC_ALL, "Portuguese");

    char buffer[LINELENGTH], address[100];
    int mapeamento, counter, i, j;
    FILE *file;
    Cache L1, L2;

    

    if(argc < 3 || strcmp(argv[1], "-h") == 0) {
        fprintf(stderr, 
        "Uso: ./<nome da compilação> [-h] <Mapeamento> <arquivo.txt>\n\n<Mapeamento> : \n"
        "\tdireto - mapeamento direto. \n\tassociativo_<Política de substituição> -> FIFO || LRU\n"
        "\n<arquivo.txt> - arquivo contendo os endereços hexadecimais para acesso a cache");
        return 0;
    }
    
    /* Write Policy */
    if(strcmp(argv[1], "direto") == 0) {
        mapeamento = 0;
        printf("Mapeamento: Direto\n");
    }
    else if(strcmp(argv[1], "associativo_FIFO") == 0) {
        mapeamento = 1;
        printf("Mapeamento:\n");
        printf("Política de substituição: LRU\n");
    }
    else if(strcmp(argv[1], "associativo_LRU") == 0) {
        mapeamento = 2;
        printf("Mapeamento:\n");
        printf("Política de substituição: LRU\n");
    }
    else {
        fprintf(stderr, "Política de mapeamento inválida.\nUso: ./<nome da compilação> [-h] <write policy> <arquivo.txt>\n");
        return 0;
    }

    file = fopen(argv[2], "r");
    if(file == NULL) {
        fprintf(stderr, "%sError%s: Could not open file.\n", Color_Red, Color_end);
        return 0; 
    }

    L1 = createCache(L1_SIZE, BLOCK_SIZE, mapeamento);
    L2 = createCache(L2_SIZE, BLOCK_SIZE, mapeamento);
    L1->nextLevel = L2;
    
    counter = 0;
    
    while(fgets(buffer, LINELENGTH, file) != NULL) {
        if(buffer[0] != '#') {
            i = 0;
            j = 0;
            
            while(buffer[i] != '\0') {
                address[j] = buffer[i];
                i++;
                j++;
            }
            
            address[j-1] = '\0';
            
            if(DEBUG) {
                printf("%i: %s\n", counter, address);
            }
            
            buscarNaCache(L1, address);
        
            counter++;
        }
    }

    printf("Num Lines: %i\n", counter);
    
    printf("L1:\n");
    printf("CACHE HITS:%s %i%s\nCACHE MISSES:%s %i%s\nMEMORY READS:%s %i%s\nMEMORY WRITES:%s %i%s\n", Color_Cyan, L1->hits, Color_end, Color_Cyan, L1->misses, Color_end
        , Color_Cyan, L1->reads, Color_end, Color_Cyan, L1->writes, Color_end);

    printf("L2:\n");
    printf("CACHE HITS:%s %i%s\nCACHE MISSES:%s %i%s\nMEMORY READS:%s %i%s\nMEMORY WRITES:%s %i%s\n", Color_Cyan, L2->hits, Color_end, Color_Cyan, L2->misses, Color_end
        , Color_Cyan, L2->reads, Color_end, Color_Cyan, L2->writes, Color_end);

    return 0;
}

Cache createCache(int tamanhoCache, int block_size, int mapeamento) {

    Cache cache;
    int i;
    
    /* Validate Inputs */
    if(tamanhoCache <= 0) {
        fprintf(stderr, "Cache deve ser maior que 0 bytes...\n");
        return NULL;
    }
    
    if(block_size <= 0) {
        fprintf(stderr, "deve ser maior que 0 bytes...\n");
        return NULL;
    }
    
    if(mapeamento != 0 && mapeamento != 1 && mapeamento != 2) {
        fprintf(stderr, "Mapeamento deve ser ou\"Direto\", \"associativo_FIFO\" ou \"associativo_LRU\".\n");
        return NULL;
    }
    
    cache = (Cache) malloc( sizeof( struct Cache_ ) );
    if(cache == NULL) {
        fprintf(stderr, "Não foi possível alocar memória para a cache.\n");
        return NULL;
    }
    
    cache->hits = 0;
    cache->misses = 0;
    cache->reads = 0;
    cache->writes = 0;
    
    cache->mapeamento = mapeamento;
    
    cache->tamanhoCache = tamanhoCache;
    cache->block_size = BLOCK_SIZE;
    
    /* Calcula numLinhas */
    cache->numLinhas = (int)(tamanhoCache / BLOCK_SIZE);
    
    cache->blocos = (Bloco*) malloc( sizeof(Bloco) * cache->numLinhas );
    assert(cache->blocos != NULL);
        
    for(i = 0; i < cache->numLinhas; i++) {
        cache->blocos[i] = (Bloco) malloc( sizeof( struct Bloco_ ) );
        cache->blocos[i]->out = 0;
        assert(cache->blocos[i] != NULL);
        cache->blocos[i]->valid = 0;
        cache->blocos[i]->tag = NULL;
    }

    cache->nextLevel = NULL;
    
    return cache;
}

int buscarNaCache(Cache cache, char* address) {
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *offset;
    int i, idxMaior, ok = 0;
    Bloco bloco;
    
    if(cache == NULL) {
        fprintf(stderr, "%sError%s: Cache vazia.\n", Color_Red, Color_end);
        return 0;
    }
    
    if(address == NULL) {
        fprintf(stderr, "%sError%s: Endereço vazio.\n", Color_Red, Color_end);
        return 0;
    }
    
    dec = HexaParaInteiro(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring);

    if(DEBUG) { 
        printf("Hex: %s\n", address);
        printf("Decimal: %u\n", dec);
        printf("Binary: %s\n", bstring);
        printf("Formatted: %s\n", bformatted);
    }

    if(cache->mapeamento == 2) {
        i = 0;
        
        tag = (char *) malloc( sizeof(char) * (TAG + 1) );
        assert(tag != NULL);
        tag[TAG] = '\0';
        
        for(i = 0; i < TAG; i++) {
            tag[i] = bformatted[i];
        }

        for(i = 0; i < cache->numLinhas; i++) {

            if(cache->blocos[i]->valid == 1 && strcmp(cache->blocos[i]->tag, tag) == 0) {
                printf("%sHIT%s\n", Color_Blue, Color_end);
                ok = 1;
                cache->blocos[i]->out = 0;
                cache->hits++;

            }
            else if(cache->blocos[i]->valid == 1) {
                cache->blocos[i]->out += 1;
            }
        }
        if(ok)
            free(tag);
        else {
            for(i = 0; i < cache->numLinhas; i++) {
                if(cache->blocos[i]->out > cache->blocos[idxMaior]->out) {
                    idxMaior = i;
                }
            }
            cache->reads++;
            cache->misses++;

            cache->blocos[idxMaior]->tag = tag;
             
            cache->blocos[idxMaior]->valid = 1;

            if(cache->nextLevel != NULL) {
                buscarNaCache(cache->nextLevel, address);
            }
            else {
                cache->writes++;
                printf("%sMISS%s\n", Color_Red, Color_end);
            }
        }
    }
    if(cache->mapeamento == 1) {
        i = 0;
        
        tag = (char *) malloc( sizeof(char) * (TAG + 1) );
        assert(tag != NULL);
        tag[TAG] = '\0';
        
        for(i = 0; i < TAG; i++) {
            tag[i] = bformatted[i];
        }

        for(i = 0; i < cache->numLinhas; i++) {

            if(cache->blocos[i]->valid == 1 && strcmp(cache->blocos[i]->tag, tag) == 0) {
                printf("%sHIT%s\n", Color_Blue, Color_end);
                ok = 1;
                cache->hits++;
            }
            else if(cache->blocos[i]->valid == 1) {
                cache->blocos[i]->out += 1;
            }
        }
        if(ok)
            free(tag);
        else {
            for(i = 0; i < cache->numLinhas; i++) {
                if(cache->blocos[i]->out > cache->blocos[idxMaior]->out) {
                    idxMaior = i;
                }
            }
            cache->reads++;
            cache->misses++;

            cache->blocos[idxMaior]->tag = tag;
             
            cache->blocos[idxMaior]->valid = 1;

            if(cache->nextLevel != NULL) {
                buscarNaCache(cache->nextLevel, address);
            }
            else {
                cache->writes++;
                printf("%sMISS%s\n", Color_Red, Color_end);
            }
        }
    }
    else if(cache->mapeamento == 0) {

        i = 0;
        
        tag = (char *) malloc( sizeof(char) * (TAG + 1) );
        assert(tag != NULL);
        tag[TAG] = '\0';
        
        for(i = 0; i < TAG; i++) {
            tag[i] = bformatted[i];
        }
        
        index = (char *) malloc( sizeof(char) * (INDEX + 1) );
        assert(index != NULL);
        index[INDEX] = '\0';
        
        for(i = TAG + 1; i < INDEX + TAG + 1; i++) {
            index[i - TAG - 1] = bformatted[i];
        }
        
        offset = (char *) malloc( sizeof(char) * (OFFSET + 1) );
        assert(offset != NULL);
        offset[OFFSET] = '\0';
        
        for(i = INDEX + TAG + 2; i < OFFSET + INDEX + TAG + 2; i++) {
            offset[i - INDEX - TAG - 2] = bformatted[i];
        }
        
        if(DEBUG) {
            printf("Tag: %s (%i)\n", tag, binarioParaDecimal(tag));
            printf("Index: %s (%i)\n", index, binarioParaDecimal(index));
            printf("Offset: %s (%i)\n", offset, binarioParaDecimal(offset));
        }
        
        bloco = cache->blocos[binarioParaDecimal(index)];
        
        if(DEBUG)
            printf("Tentando ler dado do slot %i da cache.\n", binarioParaDecimal(index));
        
        if(bloco->valid == 1 && strcmp(bloco->tag, tag) == 0) {
            printf("%sHIT%s\n", Color_Blue, Color_end);
            cache->hits++;
            free(tag);
        }
        else {
            cache->reads++;
            cache->misses++;

            bloco->tag = tag;
             
            bloco->valid = 1;

            if(cache->nextLevel != NULL) {
                buscarNaCache(cache->nextLevel, address);   
            }
            else {

                cache->writes++;
                printf("%sMISS%s\n", Color_Red, Color_end);
            }
            
        }
        
        
        free(bformatted);
        free(offset);
        free(index);
    }
    free(bstring);
    return 1;  
}

unsigned int HexaParaInteiro(const char str[]) {

    unsigned int result;
    int i;

    i = 0;
    result = 0;
    
    if(str[i] == '0' && str[i+1] == 'x') {
        i = i + 2;
    }

    while(str[i] != '\0') {
        result = result * 16;
        if(str[i] >= '0' && str[i] <= '9') {
            result = result + (str[i] - '0');
        }
        else if(tolower(str[i]) >= 'a' && tolower(str[i]) <= 'f') {
            result = result + (tolower(str[i]) - 'a') + 10;
        }
        i++;
    }

    return result;
}
 
char *getBinary(unsigned int num) {
    char* bstring;
    int i;
    
    bstring = (char*) malloc(sizeof(char) * 33);
    assert(bstring != NULL);
    
    bstring[32] = '\0';
    
    for( i = 0; i < 32; i++ ) {
        bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';
    }
    
    return bstring;
}

/* formatBinary
 * Ex. Format:
 *  -----------------------------------------------------
 * | Tag: 18 bits | Index: 12 bits | Byte Select: 4 bits |
 *  -----------------------------------------------------
 *
 * Ex. Result:
 * 000000000010001110 101111011111 00
 */

char *formatBinary(char *bstring) {
    char *formatted;
    int i;
    
    formatted = (char *) malloc(sizeof(char) * 35);
    assert(formatted != NULL);
    
    formatted[34] = '\0';
    
    for(i = 0; i < TAG; i++) {
        formatted[i] = bstring[i];
    }
    
    formatted[TAG] = ' ';
    
    for(i = TAG + 1; i < INDEX + TAG + 1; i++) {
        formatted[i] = bstring[i - 1];
    }
    
    formatted[INDEX + TAG + 1] = ' ';
    
    for(i = INDEX + TAG + 2; i < OFFSET + INDEX + TAG + 2; i++) {
        formatted[i] = bstring[i - 2];
    }

    return formatted;
}

int binarioParaDecimal(char *bin) {
    int  b, k, m, n;
    int  len, sum;

    sum = 0;
    len = strlen(bin) - 1;

    for(k = 0; k <= len; k++) {
        n = (bin[k] - '0'); 
        if ((n > 1) || (n < 0)) {
            return 0;
        }
        for(b = 1, m = len; m > k; m--) {
            b *= 2;
        }
        sum = sum + n * b;
    }
    return(sum);
}
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../io/File.h"
#include "../disk/drivers.h"

#define NUM_TESTS 2


bool test_init(){
    bool test_result = InitLLFS();
    printf("Test01: All done!\n");
    return test_result;
}

bool test_read(){
    readFile("."); //root
    printf("Test02: All done!\n");
    return true;
}


int main() {
    bool tests[NUM_TESTS];
    if(test_init()){
        tests[0] = true;
    }
    else{
        tests[0] = false;
    }
    if(test_read()){
        tests[1] = true;
    }
    else{
        tests[1] = false;
    }
    for(int k = 0; k < NUM_TESTS; k++){
        printf("Test %d: %d\n", k, tests[k]);
    }
}
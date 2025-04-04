#include <iostream>
#include <string>
#include <string.h>

using namespace std;

int main() {
    
    char str[] = "28tech-c-programming---blog---python--";

    char *token = strtok(str, "-");

    while(token != NULL) {

        printf("%s\n", token);

        token = strtok(NULL, "-");
    }

    return 0;
}
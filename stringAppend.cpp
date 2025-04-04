#include <iostream>
#include <cstring>

using namespace std;

void Append(char** str_dest, const char* str_src) {
    // Trường hợp xâu thêm vào rỗng thì kh làm gì cả
    if(str_src == NULL) return;
    
    int old_len = (*str_dest == NULL) ? 0 : strlen(*str_dest);
    int new_len = old_len + strlen(str_src);
    
    *str_dest = (char*)realloc(*str_dest, new_len*sizeof(char) + 1);
    memset(*str_dest + old_len, 0, new_len - old_len + 1);
    strcat(*str_dest, str_src);
}

int main() {
    char* str = NULL;
    Append(&str, "Ho ");
    Append(&str, "Xuan ");
    Append(&str, "Thai dep trai vlon");
    cout << str;
    free(str);
    return 0;
}
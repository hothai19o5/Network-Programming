/**
 * VD2: Lệnh gửi từ client là chuỗi ký tự có dạng “CMDX Y” trong 
 * đó CMD là các lệnh ADD, SUB, MUL,DIV, X và Y là 2 số thực. 
 * Viết đoạn chương trình kiểm tra một chuỗi ký tự có theo cú 
 * pháp trên không, xác định các giá trị của CMD, X và Y.
**/


#include <iostream>
#include <string>

using namespace std;

int main(int argc, char *argv[])
{
    if(argc != 4) {
        cout << "Error Input" << endl;
        cout << "EXAMPLE: CMD X Y" << endl;
        return 1;
    }

    string cmd = argv[1];
    float x, y;

    try{
        x = stof(argv[2]);
        y = stof(argv[3]);
    } catch (...) {
        cout << "X or Y isn't float" << endl;
        return 1;
    }

    float result;
    
    if(cmd == "ADD" || cmd == "SUB" || cmd == "MUL" || cmd == "DIV") {
        if(cmd == "ADD") result = x + y;
        if(cmd == "SUB") result = x - y;
        if(cmd == "MUL") result = x * y;
        if(cmd == "DIV") result = x / y;

        cout << "CMD: " << cmd << endl;
        cout << "X = " << x << endl;
        cout << "Y = " << y << endl;
        cout << "RESULT = " << result << endl;
    } else {
        cout << "Error Input" << endl;
        cout << "EXAMPLE: CMD X Y" << endl;
        return 1;
    }

    return 1;
}
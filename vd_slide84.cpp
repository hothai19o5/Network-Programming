#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main() {

    string target = "Ky thuat May tinh";
    string data;
    ifstream infile;

    infile.open("vd_slide84.txt");

    if(!infile.is_open()){
        cout << "Coundn't open the file" << endl;
        return 0;
    }

    while(getline(infile, data)) {
        if(data.find(target) != string::npos) {
            cout << "Found";
            infile.close();
            return 1;
        }
    }

    infile.close();
    return 0;
}
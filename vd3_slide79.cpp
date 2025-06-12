/**
 * VD3: Chuỗi ký tự sau là phản hồi của lệnh PASV trong giao thức FTP,
 * hãy xác định giá trị địa chỉ IP và cổng.
 * 227 Entering Passive Mode (213,229,112,130,216,4)
 */

#include <iostream>
#include <string>
#include <sstream>

using namespace std;

int main() {

    string res = "227 Entering Passive Mode (213,229,112,130,216,4)";

    string subRes = res.substr(27, 47);

    stringstream ss(subRes);

    int arr[6];

    string number;

    int count = 0;

    while(getline(ss, number, ',')) {
        arr[count] = stoi(number);
        count++;
    }

    cout << "IP: " << arr[0] << "." << arr[1] << "." << arr[2] << "." << arr[3] << endl;
    cout << "Port: " << arr[4] * 256 + arr[5] << endl;

    return 0;
}
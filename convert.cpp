// this program is a work in progress.
// this program will first convert a message into binary, octal, and hexadecimal.
// then it will encrypt the message.
// the final version will be object-oriented.

#include <iostream>

using namespace std;

void convertToBinary(int);
void convertToOctal(int);
void convertToHex(int);

int main()
{
    int userDec = 0;
    
    cout << "What number do you want to convert to binary, octal, and hexadecimal?: ";
    cin >> userDec;
    
    cout << "\nBinary: ";
    convertToBinary(userDec);
    cout << "\nOctal: ";
    convertToOctal(userDec);
    cout << "\nHex: ";
    convertToHex(userDec);
}

void convertToBinary(int decimalInt)
{
    if ((decimalInt / 2) != 0)
    {
        convertToBinary(decimalInt / 2);
    }
    
    cout << decimalInt % 2;
    
}

void convertToOctal(int decimalInt)
{
    if ((decimalInt / 8) != 0)
    {
        convertToOctal(decimalInt / 8);
    }
    
    cout << decimalInt % 8;
}

void convertToHex(int decimalInt)
{
    if ((decimalInt / 16) != 0)
    {
        convertToHex(decimalInt / 16);
    }
    
    cout << decimalInt % 16;
}
#include "Encrypt.h"
#include <iostream>

char Encrypt::key2(char convertedChar)
{
    char xorKey = convertedChar xor 692 xor 177;
    return xorKey;
}

int Encrypt::convertToOctal(int decimalInt)
{
    int remainder, key = 1, octalNum = 0;
    
    while (decimalInt != 0)
    {
        remainder = decimalInt % 8;
        decimalInt = decimalInt / 8;
        octalNum = octalNum + (remainder * key);
        key = key * 10;
    }
    
    return octalNum;
}

int Encrypt::convertToBinary(int xorConverted)
{
    int remainder, key = 1;
    int total = 0;
    
    while (xorConverted > 0)
    {
        remainder = xorConverted % 2;
        total = total + (key * remainder);
        xorConverted = xorConverted / 2;
        key = key * 10;
    }
    
    return total;
}

std::string Encrypt::makeNine(int binaryNumber)
{
    std::string temp = std::to_string(binaryNumber);
    
    while (temp.length() != 9)
        temp = '0' + temp;
    
    return temp;
}
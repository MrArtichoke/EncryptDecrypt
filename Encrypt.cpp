#include "Encrypt.h"
#include <iostream>

char Encrypt::key2(char convertedChar)
{
    char xorKey = convertedChar xor 692 xor 177; // flips the bits using two keys
    return xorKey;
}

int Encrypt::convertToOctal(int decimalInt)
{
    int remainder, key = 1, octalNum = 0;
    
    while (decimalInt != 0) // recursive loop for converting to octal
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
    
    while (xorConverted > 0) // recursive loop for converting to binary
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
    std::string temp = std::to_string(binaryNumber); // convert the binary number into a string
    
    while (temp.length() != 9) // makes the length of the string into 9 (for easier extraction later for decrypting)
        temp = '0' + temp;
    
    return temp;
}
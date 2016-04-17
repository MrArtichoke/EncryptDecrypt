#include "Decrypt.h"
#include <iostream>
#include <cmath>

int Decrypt::makeInt(std::string stringToConvert)
{
    int temp = atoi(stringToConvert.c_str()); // makes an into an int
    
    return temp;
}

int Decrypt::convertBack(int binaryNumber)
{
    int total = 0, remainder, key = 1;
    
    while (binaryNumber > 0) // recursive loop for converting from binary to octal
    {
        remainder = binaryNumber % 10;
        total = total + (key * remainder);
        key = key * 2;
        binaryNumber = binaryNumber / 10;
    }
    
    return total;
}

int Decrypt::convertToDecimal(int octalInt)
{
    int decimalNum = 0, key = 0, remainder;
    
    while (octalInt != 0) // recursive loop for converting from octal to decimal
    {
        remainder = octalInt % 10;
        octalInt = octalInt / 10;
        decimalNum = decimalNum + (remainder * pow(8, key));
        ++key;
    }
    
    return decimalNum;
}

char Decrypt::key2(char convertedChar) // uses the same xor gates to do decryption because xor is its own inverse
{
    char xorKey = convertedChar xor 692 xor 177;
    return xorKey;
}


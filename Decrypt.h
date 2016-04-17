#ifndef DECRYPT_H
#define DECRYPT_H

#include <iostream>

class Decrypt {
public:
    int makeInt(std::string); // convert the string back into an integer
    int convertBack(int); // convert the integer back from binary to octal
    int convertToDecimal(int); // convert from octal back into decimal form
    char key2(char); // convert from decimal into characters
};

#endif
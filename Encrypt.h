#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <iostream>

class Encrypt {
public:
    int convertToOctal(int); // key for converting char into octal
    char key2(char); // key for converting chars into different chars
    int convertToBinary(int); // key for converting octal to binary
    std::string makeNine(int); // converts binary to a string and adds a 0 in front
};

#endif
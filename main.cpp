#include <iostream>
#include <fstream>
#include <ostream>
#include "Encrypt.h"
#include "Decrypt.h"

std::string *doEncryption(std::string &); // encryption helper function
std::string doDecryption(std::string &); // decryption helper function
void freeMemory(std::string *); // frees the memory we allocated

int main()
{
    char choice;
    std::string userString;
    std::string *ptr = NULL;
    std::string makeNewFile;
    std::ofstream fout;
    std::ifstream ifstr;
    std::string getFile;
    bool loop = true;
    
    while (loop)
    {
        std::cout << "------MENU------\n";
        std::cout << "1. Encrypt from a file\n" <<
        "2. Decrypt from an encrypted file\n" <<
        "3. Enter a string to Encrypt and make Encrypted file\n" <<
        "4. Exit program\n\n";
        std::cin >> choice;
        std::cin.ignore(); //need this because we use getline() later
        
        switch(choice)
        {
            case '1':
                std::cout << "\nEnter the filename: ";
                std::getline(std::cin, getFile);
                
                try // exception handling -- if there is no file with the name the user enters, throw an exception
                {
                    ifstr.open(getFile.c_str());
                    if (!ifstr.good())
                        throw "\nERROR READING FILE!";
                    else
                        std::cout << "\nEncrypted: \n";
                }
                catch (const char *errMsg)
                    { std::cerr << errMsg; }
                
                while (ifstr.good())
                    std::getline(ifstr, userString);
                ptr = doEncryption(userString);
                std::cout << std::endl;
                ifstr.close();
                userString = "";
                break;
            case '2':
                std::cout << "\nEnter the filename: ";
                std::getline(std::cin, getFile);
                
                try
                {
                    ifstr.open(getFile.c_str());
                    if (!ifstr.good())
                        throw "\nERROR READING FILE!";
                    else
                        std::cout << "\nDecrypted: \n";
                }
                catch (const char *errMsg)
                    { std::cerr << errMsg; }
                
                while (ifstr.good())
                    std::getline(ifstr, userString);
                doDecryption(userString);
                std::cout << std::endl <<std::endl;
                ifstr.close();
                userString = "";
                break;
            case '3':
                std::cout << "\nEnter a string to Encrypt: ";
                std::getline(std::cin, userString);
                ptr = doEncryption(userString);
                std::cout << "\nEnter the file name you want to output encrypted content to: ";
                std::getline(std::cin, makeNewFile);
                fout.open(makeNewFile.c_str(), std::ios::app); // makes a new file for us out of the string we enter, so we can test to decrypt it later in the menu
                for (int i = 0; i < userString.size(); i++)
                    fout << ptr[i];
                fout.close();
                std::cout << "\n\nFile " << makeNewFile << " created!\n\n";
                userString = "";
                break;
            case '4':
                loop = false;
                break;
            default:
                std::cout << "\nInvalid choice!\n\n";
        }
    }
    
    if (ptr!=NULL) // free the dynamically allocated memory
        freeMemory(ptr);
}

std::string *doEncryption(std::string &userString)
{
    char theEncoder[userString.size()];
    int conversionInt[userString.size()];
    
    Encrypt newEncrypt;
    
    std::string *ptr = new std::string[userString.size()];
    
    for (int i = 0; i < userString.size(); i++)
    {
        theEncoder[i] = userString[i];
        theEncoder[i] = newEncrypt.key2(theEncoder[i]); // encrypt using the xor gates
        conversionInt[i] = theEncoder[i];
        ptr[i] = newEncrypt.makeNine(newEncrypt.convertToBinary(newEncrypt.convertToOctal(conversionInt[i]))); // multiple function call to convert to octal, then binary, then add a 0 to the front
        std::cout << ptr[i];
    }
    
    std::cout << std::endl;
    
    return ptr;
}

std::string doDecryption(std::string &userString)
{
    char theEncoder[userString.size()];
    int conversionInt[userString.size()];
    
    Decrypt newDecrypt;
    
    std::string *ptr = new std::string[userString.size()]; // need to allocate memory to make a bunch of new strings
    std::string placeholder;
    
    for (int i = 0; i < userString.size(); i = i + 9)
    {
        ptr[i] = userString.substr(i, 9); // cut each piece of the string into 9 bits
        conversionInt[i] = newDecrypt.makeInt(ptr[i]);
        conversionInt[i] = newDecrypt.convertToDecimal(newDecrypt.convertBack(conversionInt[i]));
        theEncoder[i] = conversionInt[i];
        theEncoder[i] = newDecrypt.key2(theEncoder[i]);
        std::cout << theEncoder[i];
        placeholder = placeholder + theEncoder[i];
    }
    return placeholder;
}

void freeMemory(std::string *ptr)
{
    std::cout << "\nFreeing allocated memory...\n";
    delete [] ptr;
    ptr = NULL;
}
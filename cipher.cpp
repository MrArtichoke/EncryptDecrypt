// this program is a work in progress.
// this program will first convert a message into binary, octal, and hexadecimal.
// then it will encrypt the message.
// the final version will be object-oriented.


#include <iostream>
#include <cstring>
#include <string>
#include <ctime>
#include <cstdlib>

using namespace std;

int createBinary(int);
string intToString(int);
string makeCipher(string);
char newChar(char);

int main()
{
    srand(static_cast<unsigned int>(time(0)));

    int toConvert = 0;
    int toTestChar = 0;
    char convertIt;
    string converted;
    string word;
    string newWord;
    
    cout << "Word?: ";
    getline(cin, word);
    
    newWord = makeCipher(word);
    cout << newWord << endl;
    
    cout << "Number?: ";
    cin >> toConvert;
    
    converted = intToString(createBinary(toConvert));
    
    cout << "Binary number: " << converted << endl;
    cout << "Testing length of binary number is 8: ";
    
    if (converted.length() == 8)
    {
        cout << "True!" << endl;
    }
    else
    {
        cout << "False!" << endl;
    }
    
    cout << "Char for number conversion?: ";
    cin >> convertIt;
    toTestChar = convertIt;
    
    cout << toTestChar << endl;
    
    string newToTestChar = intToString(createBinary(toTestChar));
    
    cout << newToTestChar << endl;
}

int createBinary(int decimal)
{
    int remainder = 0;
    int binaryNum = 0;
    int temp = 1;
    
    while (decimal !=0)
    {
        remainder = decimal % 2;
        decimal = decimal / 2;
        binaryNum = binaryNum + (remainder * temp);
        temp = temp * 10;
    }
    return binaryNum;
}

string intToString(int binaryNumber)
{
    string binaryString = to_string(binaryNumber);
    if (binaryString.length() == 8)
    {
        binaryString = binaryString;
    }
    else if (binaryString.length() == 7)
    {
        binaryString = "0" + binaryString;
    }
    else if (binaryString.length() == 6)
    {
        binaryString = "00" + binaryString;
    }
    else if (binaryString.length() == 5)
    {
        binaryString = "000" + binaryString;
    }
    else if (binaryString.length() == 4)
    {
        binaryString = "0000" + binaryString;
    }
    else if (binaryString.length() == 3)
    {
        binaryString = "00000" + binaryString;
    }
    else if (binaryString.length() == 2)
    {
        binaryString = "000000" + binaryString;
    }
    else
    {
        binaryString = "0000000" + binaryString;
    }
    return binaryString;
}

string makeCipher(string toCipher)
{
    char stringToChar[toCipher.length()];
    int numZero = 0;
    for (int i = 0; i < toCipher.length(); i++)
    {
        stringToChar[i] = toCipher[i];
    }
    for (int i = 0; i < toCipher.length(); i++)
        if (stringToChar[i] == ' ')
        {
            stringToChar[i] = newChar(stringToChar[i]);
        }
    for (int i = 0; i < toCipher.length(); i++)
    {
        toCipher[i] = stringToChar[i];
    }
    return toCipher;
}

char newChar(char randomChar)
{
    int temp = 2 + rand() % 8;
    randomChar = temp + '0';
    return randomChar;
}
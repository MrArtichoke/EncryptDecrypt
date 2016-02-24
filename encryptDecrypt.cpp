// this program is a work in progress.
// this program will first convert a message into binary, octal, and hexadecimal.
// then it will encrypt the message.
// the final version will be object-oriented.


#include <iostream>
#include <cmath>

using namespace std;

char encryption(char &);

int main()
{
    string userString = "";
    cout << "Enter a string to encrypt: ";
    getline(cin, userString);
    
    char theEncoder[userString.size()];
    
    for (int i = 0; i < userString.size(); i++)
    {
        theEncoder[i] = userString[i];
        cout << encryption(theEncoder[i]);
    }
}

char encryption(char &key)
{
    if (isalpha(key))
    {
        if (key == 'A' || key == 'a')
            key = '^';
        else if (key == 'B' || key == 'b')
            key = '@';
        else if (key == 'D' || key == 'd')
            key = '+';
        else if (key == 'H' || key == 'h')
            key = '/';
        else if (key == 'P' || key == 'p')
            key = '%';
        else
            key = key - 34;
    }
    
    else if (key == ' ')
        key = '*';
    
    else
    {
        if (key == '0')
            key = '5';
        else if (key == '1')
            key = '6';
        else if (key == '2')
            key = '4';
        else if (key == '3')
            key = '7';
        else if (key == '4')
            key = '3';
        else if (key == '5')
            key = '8';
        else if (key == '6')
            key = '2';
        else if (key == '7')
            key = '9';
        else if (key == '8')
            key = '1';
        else
            key = '0';
    }
    
    return key;
}
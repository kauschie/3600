/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 1/25/24
Assignment: Lab 1

*/


#include <iostream>
#include <fstream>


using namespace std;

int main()
{
    char uname[100];
    int num;

    cout << "Enter your name: ";
    cin >> uname;

    cout << "Enter a number: ";
    cin >> num;

    ofstream fout("log");
    if (!fout) {
        cout << "Unable to open log... quitting now.\n";
        return 1;
    }

    int sum = 0;
    for (int i = 1; i < num; i++) {
        sum += i;
    }

    fout << uname << "\n" << sum << endl;

    fout.close();

    return 0;
}

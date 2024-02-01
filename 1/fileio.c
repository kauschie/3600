/*
Name: Michael Kausch
Class: Operating Systems
Professor: Gordon
Date: 1/25/24
Assignment: Lab 1

*/


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
    char uname[100];
    char tmp[100];
    int num;
    int sum;
    int fd;
    int i;

    /*need to define all vars first before any code in C*/

    /*
    cout << "Enter your name: ";
    cin >> uname;

    cout << "Enter a number: ";
    cin >> num;
    */
    printf("Enter your name: ");
    scanf("%s", uname);
    printf("Enter a number: ");
    scanf("%d", &num);

    fd = open("log", O_WRONLY | O_CREAT, 0660); /*octal number begin with 0*/


    /*
    ofstream fout("log");
    if (!fout) {
        cout << "Unable to open log... quitting now.\n";
        return 1;
    }
    */

    for (i = 1; i <= num; i++) {
        sum += i;
    }

    /*fout << uname << "\n" << sum << endl;*/
    write(fd, uname, strlen(uname)); 
    sprintf(tmp, "\n%d\n", sum);
    write(fd, tmp, strlen(tmp)); 

    /*
    fout.close();
    */
    close(fd);

    
    return 0;
}

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>


//original terminal attributes in orig_termios
struct termios orig_termios;


// yup, disable
void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


// raw mode - this works byte-by-byte, or char-by-char. 
// no echo (like you typing password with sudo, which can mean, text don't print on the screen),
// in raw mode out character transfer to our program immediately, we don't need to press ENTER, 
// no buffer for our typing and our specific symbols processed by out program, not by a system like in the canonical


// canonical mode - this works line-by-line, which can mean, the system wait for press ENTER, 
// supports specific symbols like CTRL-C

void enableRawMode()
{
    // with tcgetattr we get terminal attributes
    tcgetattr(STDIN_FILENO, &orig_termios);
   
   
    // atexit from stdlib, we use it to register our disable mode function to be called automatically    
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    

    // c_lflag field for "local flags", which describe behavior of terminal i/o on user level, signals, echo,
    //  print what we types etc.
    // c_iflag = input flags
    // c_oflag = output flags

    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN  | ISIG);
   
   
    // with tcsetattr we set the new attributes after modifying
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int main()
{
     enableRawMode();

     char c;
     while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
     {
        // iscnrtl from ctype.h and it check whether a character is a control character 
        if(iscntrl(c))
        {
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d ('%c')\r\n", c, c);
        }
     }
     return 0;
}

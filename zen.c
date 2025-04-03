#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

//original terminal attributes in orig_termios
struct termios orig_termios;



// error handling
void die(const char *s)
{   
    perror(s);
    exit(1);
}





// yup, disable
void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
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
   
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
   
    // atexit from stdlib, we use it to register our disable mode function to be called automatically    
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    

    // c_lflag field for "local flags", which describe behavior of terminal i/o on user level, signals, echo,
    //  print what we types etc.
    // c_iflag = input flags
    // c_oflag = output flags

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP  | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN  | ISIG);
    

    // VMIN and VTIME from termios.h ,  c_cc it's control characters, an array of bytes 
    // that control various terminal settings
    //
    // VMIN value sets the minimum number of bytes of input needed before read()  can return
    // VTIME value sets the maximum amount of time to wait before read() returns   
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;   
   
    // with tcsetattr we set the new attributes after modifying
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}


int main()
{
     enableRawMode();

     while(1)
     {
         char c = '\0';
         read(STDIN_FILENO, &c, 1);
         if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
             die("read");

         // iscnrtl from ctype.h and it check whether a character is a control character 
         if(iscntrl(c))
         {
             printf("%d\r\n", c);
         }
         else 
         {
             printf("%d ('%c')\r\n", c, c);
         }
         if (c == 'q') break;
     }
      
     return 0;
}

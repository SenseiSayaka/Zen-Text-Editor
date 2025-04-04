// *** includes *** //
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

// *** defines *** //

#define ZEN_VERSION "1.0"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}


// *** data *** //


// editor state
struct editorConfig
{
    int cx;  // cursor's x pos
    int cy;  // cursor's y pos
                    
    int screenrows;  // row
    int screencols;  // columns
   
    //original terminal attributes in orig_termios
    struct termios orig_termios;

};

enum editorKey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct editorConfig E;



// *** terminal *** //


// error handling
void die(const char *s)
{   
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
}

// yup, disable
void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
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
   
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
   
    // atexit from stdlib, we use it to register our disable mode function to be called automatically    
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    

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

int editorReadKey()
{
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno !=EAGAIN)
            die("read");
    }
    
    if(c == '\x1b')
    {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[')
        {
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~')
                {
                    switch(seq[1])
                    {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;          
                    }
                }
            }
            else
            {
            switch(seq[1])
            {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN; 
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
        } 
        else if(seq[0] == 'O')
        {
            switch(seq[1])
            {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';

    }   else
        {
            return c;
        }        
}


int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else 
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


// *** append buffer *** //

struct abuf
{
    char *b;
    
    int len;
};

void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}


void abFree(struct abuf *ab)
{
    free(ab->b);
}


// *** output *** //

void editorDrawRows(struct abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        if(y == E.screenrows / 3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Zen Version -- version %s", ZEN_VERSION);
            if(welcomelen > E.screencols) welcomelen = E.screencols;
            
            int padding = (E.screencols - welcomelen) / 2;
            if(padding)
            {
                abAppend(ab, "~", 1);
                padding--;
            }
            while(padding--)
                abAppend(ab, " ", 1);

            abAppend(ab, welcome, welcomelen);
        }
        else
        {

            abAppend(ab, "~", 1);
        }

        // clear line when redraw instead clear whole terminal

        abAppend(ab, "\x1b[K", 3);

        if (y < E.screenrows - 1)
        {
            
            abAppend(ab, "\r\n", 2);
        }
    }
}


// in this section we uses escape sequences to clear the screen, move the cursor, formatting print , etc.
void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);


    //move cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// *** input *** //

void editorMoveCursor(int key)
{
    switch(key)
    {
        case ARROW_LEFT:
            if(E.cx != 0)
            {
                E.cx--;
            }
            break;
        case ARROW_RIGHT:
            if(E.cx != E.screencols - 1)
            {
                E.cx++;
            } 
            break;
        case ARROW_UP:
            if(E.cy != 0)
            {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy != E.screenrows - 1)
            {
                E.cy++;
            }
            break;
    }
}

void editorProcessKeypress()
{
    int c = editorReadKey();

    switch(c)
    {
        case CTRL_KEY('q'):

    
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
    
            exit(0);
            break;
        
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screencols - 1;
            break;
    
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while(times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

    }

}

// *** init *** //

void initEditor()
{
    E.cy = 0;
    E.cx = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main()
{
     enableRawMode();
     initEditor();

     while(1)
     {
         editorRefreshScreen();
         editorProcessKeypress();
     }
      
     return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>

#include <termios.h>
#include <string.h>     // needed for memset
#include <math.h>       // For signal dB computation

#include "usb_backpack.h"

const int ms = 1000;

display_20x4_lcd* make_20x4_lcd(display_20x4_lcd* d, char* port, bool new_lcd, float fs)
{
    d = (display_20x4_lcd*) malloc(sizeof(display_20x4_lcd));

    memset(&(d->tio),0,sizeof(struct termios));
    d->tio.c_iflag=0;
    d->tio.c_oflag=0;
    d->tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
    d->tio.c_lflag=0;
    d->tio.c_cc[VMIN]=1;
    d->tio.c_cc[VTIME]=2;

    int psz = strlen(port);
    for(int i = 0; i < psz; i++)
    {
        d->tty_port[i] = port[i];
    }

    d->tty_fd = open(d->tty_port, O_RDWR | O_NONBLOCK);
    cfsetospeed(&(d->tio),B115200);            // 115200 baud
    cfsetispeed(&(d->tio),B115200);            // 115200 baud

    tcsetattr(d->tty_fd,TCSANOW,&(d->tio));

    //Clear display lines
    int i = 0;
    for(int i = 0; i < LCD_COLS; i++)
    {
        d->line1[i] = ' ';
        d->line2[i] = ' ';
        d->line3[i] = ' ';
        d->line4[i] = ' ';
    }

    d->line1[i] = '\0';
    d->line2[i] = '\0';
    d->line3[i] = '\0';
    d->line4[i] = '\0';

    //Commands to LCD
    unsigned char cmd[4];
    for(i = 0; i < 4; i++)
        cmd[i] = ' ';
    cmd[0] = 0xFE;

    //Default don't use autoscroll function
    cmd[1] = 0x52; //0x52 off, 0x51 on
    write(d->tty_fd,cmd,2);
    usleep(10*ms);

    //Configure display size
    cmd[1] = 0xD1;
    cmd[2] = 20; // 20 columns
    cmd[3] = 4; // 4 row
    if(new_lcd)
    {
        write(d->tty_fd, cmd, 4);
        usleep(10*ms);

        // Set custom startup splash
        cmd[1] = 0x40; //overwrite splash

        sprintf(d->line1,"THE Transmogrifox   ");
        sprintf(d->line2,"Transmogrification  ");
        sprintf(d->line3,"Station Guitar FX   ");
        sprintf(d->line4,"Transmogrifox's     ");

        write(d->tty_fd, d->line1, LCD_COLS);
        write(d->tty_fd, d->line2, LCD_COLS);
        write(d->tty_fd, d->line3, LCD_COLS);
        write(d->tty_fd, d->line4, LCD_COLS);
        usleep(10*ms);
        write(d->tty_fd, cmd, 2);
        usleep(10*ms);

        //Set splash delay
        cmd[1] = 0x41; //
        cmd[2] = 6; //x1/4 second
        write(d->tty_fd,cmd,3);
        usleep((1+cmd[2])*250*ms);

        //DEBUG
        sprintf(d->line1,"Configuration Finish");
        write(d->tty_fd, d->line1, LCD_COLS);
    } else
    {
        //Clear screen
        cmd[1] = 0x58;
        write(d->tty_fd, cmd, 2);
        usleep(10*ms);
    }

    cmd[1] = 0x58;
    write(d->tty_fd, cmd, 2);
    usleep(10*ms);
    
    //Contrast
    cmd[1] = 0x50;
    cmd[2] = 200;
    write(d->tty_fd, cmd, 3);
    usleep(10*ms);

    tcdrain(d->tty_fd);
    usleep(10*ms);
    //Peak detector
    for(int i=0; i < LCD_N_PK_DET; i++)
        d->pk_detector[i] = 0.0;

    return d;
}

int lcd_write_line(display_20x4_lcd* d, char* string, unsigned char line,unsigned char pos)
{
    char cmd[4];

    int sz = strlen(string);
    unsigned char p = pos;
    unsigned char l = line;
    if(p > LCD_COLS)
    {
        p = LCD_COLS;
        l += 1;
        if(l > 4)
            l = 0;
    }
    int e = p + sz;
    if (e > LCD_COLS)
        e = LCD_COLS;
    //Move cursor position
    cmd[0] = 0xFE;
    cmd[1] = 0x47;
    cmd[2] = p;
    cmd[3] = l;

    write(d->tty_fd, cmd, 4);
    usleep(10*ms);

    write(d->tty_fd, string, e-p);
    tcdrain(d->tty_fd);
    usleep(10*ms);

    return (e-p);
}

void lcd_clear_display(display_20x4_lcd* d)
{
    char cmd[2];
    //Clear screen
    cmd[0] = 0xFE;
    cmd[1] = 0x58;
    write(d->tty_fd, cmd, 2);
    tcdrain(d->tty_fd);
    usleep(10*ms);
}

void lcd_read_from_device(display_20x4_lcd* d)
{
    unsigned char c = 0;
    int i = 0;
    unsigned char x = 0;
    while (read(d->tty_fd,&c,1) > 0)
    {
        printf("%c", c);
        if( ++i > 4 )
        {
            x = (c  - '0') & 0x0F;
            printf("\nState: %d\n", x);
            for(int j = 0; j < 4; j++)
            {
                printf("%d", ((x >> j) & 0x01) );
            }
            printf("\n");
            i = 0;
        }
    }
}

void lcd_color(display_20x4_lcd* d, unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char cmd[5];
    cmd[0] = 0xFE;
    cmd[1] = 0xD5;
    cmd[2] = r;
    cmd[3] = g;
    cmd[4] = b;
    write(d->tty_fd, cmd, 5);
    tcdrain(d->tty_fd);
    usleep(10*ms);
}

// Give it a signal bounded (-1.0 to 1.0) and tell whether it is input (true) or output (false)
void
lcd_level_meter(display_20x4_lcd* d, float *signal, int N, unsigned char chan)
{
    int i;
    float x = 0.0;

    for(i=0; i<N; i++)
    {
        x = fabs(signal[i]);
        if(x > d->pk_detector[chan])
        {
            d->pk_detector[chan] = x;
        }
    }
}

void
lcd_level_meter_write(display_20x4_lcd* d, unsigned char chan)
{
    unsigned char cmd[4];
    cmd[0] = 0xFE;
    cmd[1] = 0xD6; //audio level meter
    cmd[2] = chan; //Channel
    cmd[3] = 0x00; //Level

    // int l0 = (int) (40.0 + (20.0*log10(d->pk_detector[chan])));
    // l0 /= 2;
    int l0 = (int) (20.0*d->pk_detector[chan]);

    if(l0 > 0x1F)
        l0 = 0x1F;
    if(l0 < 0)
        l0 = 0;


    if(d->pk_detector[chan] > 0.95)
    {
        l0 = 0x1F;
    }

    cmd[3] = (unsigned char) (l0&0xFF); //Level
    write(d->tty_fd, cmd, 4);
    tcdrain(d->tty_fd);

    d->pk_detector[chan] = 0.0;

    //printf("SIG = %f , %f , %f, %d, %d\n", x, d->pk_detector[0], d->pk_detector[1], l0, l1);
}

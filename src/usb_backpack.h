#include <unistd.h>
#include <termios.h>

#define _POSIX_SOURCE 1

#define LCD_ROWS        4
#define LCD_COLS        20
#define LCD_N_PK_DET    LCD_ROWS*2

//Intended for sending messages to Adafruit USB backpack display
typedef struct display_20x4_lcd_t
{
    float fs, ifs;

    // termios stuff
    struct termios tio;
    char tty_port[256];
    int tty_fd;

    // Default message
    char line1[LCD_COLS + 1];
    char line2[LCD_COLS + 1];
    char line3[LCD_COLS + 1];
    char line4[LCD_COLS + 1];

    //level_meter
    float pk_detector[LCD_N_PK_DET];

} display_20x4_lcd;

//Use "new_lcd" (true) to configure the new LCD for the first time.
//Set it to false thereafter (although there is no harm repeating this)
display_20x4_lcd* make_20x4_lcd(display_20x4_lcd* d, char* port, bool new_lcd, float fs);

void lcd_clear_display(display_20x4_lcd*);
void lcd_clear_line(display_20x4_lcd*, int line);
void lcd_set_new_lcd(display_20x4_lcd*);

// Write something to a specific position in a line without overwriting the
// entire line.
int lcd_write_line(display_20x4_lcd*, char* string, unsigned char line,unsigned char pos);

// Give it a signal bounded (-1.0 to 1.0) and tell whether it is input (true) or output (false)
void lcd_level_meter(display_20x4_lcd* d, float *signal, int N, unsigned char chan);
void lcd_level_meter_write(display_20x4_lcd* d, unsigned char chan);


// Set BG color
void lcd_color(display_20x4_lcd* d,unsigned char r, unsigned char g, unsigned char b);

void lcd_read_from_device(display_20x4_lcd* d);

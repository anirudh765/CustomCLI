#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 4096
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1000
#define CTRL_KEY(k) ((k) & 0x1f)

// Data structures
typedef struct {
    char **lines;
    int count;
    int capacity;
    char *filename;
    int modified;
} TextBuffer;

// Terminal handling
struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Buffer operations
TextBuffer* buffer_create() {
    TextBuffer *buffer = malloc(sizeof(TextBuffer));
    buffer->count = 0;
    buffer->capacity = MAX_LINES;
    buffer->filename = NULL;
    buffer->modified = 0;
    
    buffer->lines = malloc(sizeof(char*) * buffer->capacity);
    for (int i = 0; i < buffer->capacity; i++) {
        buffer->lines[i] = NULL;
    }
    
    
    buffer->lines[0] = malloc(MAX_LINE_LENGTH);
    buffer->lines[0][0] = '\0';
    buffer->count = 1;
    
    return buffer;
}

void buffer_free(TextBuffer *buffer) {
    if (buffer == NULL) return;
    
    for (int i = 0; i < buffer->count; i++) {
        if (buffer->lines[i] != NULL) {
            free(buffer->lines[i]);
        }
    }
    
    free(buffer->lines);
    if (buffer->filename != NULL) {
        free(buffer->filename);
    }
    free(buffer);
}

void buffer_insert_line(TextBuffer *buffer, int at, char *line) {
    if (at < 0 || at > buffer->count) return;
    
    if (buffer->count >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->lines = realloc(buffer->lines, sizeof(char*) * buffer->capacity);
    }
    
  
    for (int i = buffer->count; i > at; i--) {
        buffer->lines[i] = buffer->lines[i-1];
    }
    
    buffer->lines[at] = malloc(MAX_LINE_LENGTH);
    strcpy(buffer->lines[at], line);
    buffer->count++;
    buffer->modified = 1;
}

void buffer_delete_line(TextBuffer *buffer, int at) {
    if (at < 0 || at >= buffer->count) return;
    
    free(buffer->lines[at]);
    
   
    for (int i = at; i < buffer->count - 1; i++) {
        buffer->lines[i] = buffer->lines[i+1];
    }
    
    buffer->count--;
    buffer->modified = 1;
}

// File operations
int buffer_open(TextBuffer *buffer, const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
      
        buffer->filename = strdup(filename);
        return 0;
    }
    
    char temp[MAX_LINE_LENGTH];
    char c;
    int i = 0;
    int line_count = 0;
    
    
    for (int j = 0; j < buffer->count; j++) {
        free(buffer->lines[j]);
    }
    buffer->count = 0;
    
    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            temp[i] = '\0';
            if (buffer->count >= buffer->capacity) {
                buffer->capacity *= 2;
                buffer->lines = realloc(buffer->lines, sizeof(char*) * buffer->capacity);
            }
            buffer->lines[line_count] = malloc(MAX_LINE_LENGTH);
            strcpy(buffer->lines[line_count], temp);
            line_count++;
            i = 0;
        } else {
            if (i < MAX_LINE_LENGTH - 1) {
                temp[i++] = c;
            }
        }
    }
    
    // Handle the last line if it doesn't end with newline
    if (i > 0) {
        temp[i] = '\0';
        if (buffer->count >= buffer->capacity) {
            buffer->capacity *= 2;
            buffer->lines = realloc(buffer->lines, sizeof(char*) * buffer->capacity);
        }
        buffer->lines[line_count] = malloc(MAX_LINE_LENGTH);
        strcpy(buffer->lines[line_count], temp);
        line_count++;
    }
    
    buffer->count = line_count;
    if (buffer->count == 0) {
     
        buffer->lines[0] = malloc(MAX_LINE_LENGTH);
        buffer->lines[0][0] = '\0';
        buffer->count = 1;
    }
    
    buffer->filename = strdup(filename);
    buffer->modified = 0;
    
    close(fd);
    return 1;
}

int buffer_save(TextBuffer *buffer) {
    if (buffer->filename == NULL) {
        return 0;
    }
    
    int fd = open(buffer->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        return 0;
    }
    
    for (int i = 0; i < buffer->count; i++) {
        write(fd, buffer->lines[i], strlen(buffer->lines[i]));
        if (i < buffer->count - 1) {
            write(fd, "\n", 1);
        }
    }
    
    close(fd);
    buffer->modified = 0;
    return 1;
}

// Display functions
void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void display_buffer(TextBuffer *buffer, int cursor_x, int cursor_y) {
    clear_screen();
    
    for (int i = 0; i < buffer->count; i++) {
        write(STDOUT_FILENO, buffer->lines[i], strlen(buffer->lines[i]));
        write(STDOUT_FILENO, "\r\n", 2);
    }
    

    char status[80];
    snprintf(status, sizeof(status), "--- %s %s ---",
             buffer->filename ? buffer->filename : "[No Name]",
             buffer->modified ? "(modified)" : "");
             
    char cursor_pos[32];
    snprintf(cursor_pos, sizeof(cursor_pos), "\x1b[%d;%dH", cursor_y + 1, cursor_x + 1);
    write(STDOUT_FILENO, cursor_pos, strlen(cursor_pos));
}

// Main editor loop
void editor_loop(TextBuffer *buffer) {
    int cursor_x = 0;
    int cursor_y = 0;
    
    while (1) {
        display_buffer(buffer, cursor_x, cursor_y);
        
        char c;
        if (read(STDIN_FILENO, &c, 1) == -1) {
            continue;
        }
        
        if (c == CTRL_KEY('q')) {
            if (buffer->modified) {
                
                char prompt[80] = "File has been modified. Save before quitting? (y/n)";
                clear_screen();
                write(STDOUT_FILENO, prompt, strlen(prompt));
                
                char answer;
                read(STDIN_FILENO, &answer, 1);
                if (answer == 'y' || answer == 'Y') {
                    buffer_save(buffer);
                }
            }
            break;
        } else if (c == CTRL_KEY('s')) {
            buffer_save(buffer);
        } else if (c == '\x1b') {
            
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return;
            
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': 
                        if (cursor_y > 0) cursor_y--;
                        break;
                    case 'B': 
                        if (cursor_y < buffer->count - 1) cursor_y++;
                        break;
                    case 'C': 
                        if (cursor_x < strlen(buffer->lines[cursor_y])) cursor_x++;
                        break;
                    case 'D': 
                        if (cursor_x > 0) cursor_x--;
                        break;
                }
            }
        } else if (c == '\r') {
           
            char *current_line = buffer->lines[cursor_y];
            char new_line[MAX_LINE_LENGTH];
            strcpy(new_line, current_line + cursor_x);
            current_line[cursor_x] = '\0';
            
            buffer_insert_line(buffer, cursor_y + 1, new_line);
            cursor_y++;
            cursor_x = 0;
        } else if (c == 127 || c == '\b') {
            
            if (cursor_x > 0) {
                memmove(&buffer->lines[cursor_y][cursor_x - 1], 
                        &buffer->lines[cursor_y][cursor_x], 
                        strlen(&buffer->lines[cursor_y][cursor_x]) + 1);
                cursor_x--;
                buffer->modified = 1;
            } else if (cursor_y > 0) {
                
                int prev_len = strlen(buffer->lines[cursor_y - 1]);
                strcat(buffer->lines[cursor_y - 1], buffer->lines[cursor_y]);
                buffer_delete_line(buffer, cursor_y);
                cursor_y--;
                cursor_x = prev_len;
            }
        } else if (isprint(c)) {
           
            char *line = buffer->lines[cursor_y];
            int len = strlen(line);
            
            if (len < MAX_LINE_LENGTH - 1) {
                memmove(&line[cursor_x + 1], &line[cursor_x], len - cursor_x + 1);
                line[cursor_x] = c;
                cursor_x++;
                buffer->modified = 1;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    enable_raw_mode();
    
    TextBuffer *buffer = buffer_create();
    
    if (argc >= 2) {
        buffer_open(buffer, argv[1]);
    }
    
    editor_loop(buffer);
    
    buffer_free(buffer);
    disable_raw_mode();
    clear_screen();
    
    return 0;
}
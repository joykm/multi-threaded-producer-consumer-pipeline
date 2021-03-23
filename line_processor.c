# define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // strcmp
#include <pthread.h> // threads
#include <unistd.h>

// Global variables for shared thread access
#define END_MARKER "STOP\n"
char buffer1[50][1001]; 
char buffer2[50][1001];
char buffer3[(50 * 1000) + 2];

// Number of unexamined lines in buffer1
int b1_line_count = 0;
pthread_mutex_t b1_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t b1_full = PTHREAD_COND_INITIALIZER;

// Number of unexamined lines in buffer2
int b2_line_count = 0;
pthread_mutex_t b2_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t b2_full = PTHREAD_COND_INITIALIZER;

// Number of unprinted characters in buffer3
int b3_char_count = 0;
pthread_mutex_t b3_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t b3_full = PTHREAD_COND_INITIALIZER;


/*
* Input user input lines one line at a time.
*/
void* input(void* args) {

    // Initialize variables for current line
    size_t len = 0;
    ssize_t nread = 0;
    char* currLine = NULL;

    // Keep gathering user input until END_MARKER is detected
    int i = 0;
    while ((nread = getline(&currLine, &len, stdin)) != -1) {
        
        /* Buffer1 critical section start, lock b1_mutex before adding data
        and adjusting b1_line_count */
        pthread_mutex_lock(&b1_mutex);

        strcpy(&buffer1[i][0], currLine);
        b1_line_count++;

        /* Buffer1 critical section end, signal lineSeparatorThread that buffer 
        has new data and unlock b1_mutex  */
        pthread_cond_signal(&b1_full);
        pthread_mutex_unlock(&b1_mutex);
        
        // If END_MARKET is encountered, stop reading input in
        if (strcmp(currLine, END_MARKER) == 0) {
            break;
        } 

        // Iterate i for next input line
        i++;
    }

    // Free dynamic memory allocated for currLine
    free(currLine);
    return NULL;
}

/*
* Replace all "/n" with " "
*/
void* lineSeparator(void* args) {
    
    // Iterate buffer1 to "consumer" buffer1 and "produce" buffer2
    int i = 0;
    while(1) {
      
        /* Buffer1 critical section start, lock b1_mutex before
        checking if buffer has any new data or adjusting b1_line_count */
        pthread_mutex_lock(&b1_mutex);
        while (b1_line_count == 0) {
            pthread_cond_wait(&b1_full, &b1_mutex);
        }

        /* Buffer2 critical section start, lock b2_mutex before
        inserting any data and adjusting b2_line_count */
        pthread_mutex_lock(&b2_mutex);
        strcpy(buffer2[i], buffer1[i]);
        b1_line_count--;

        // End critical section for buffer1, unlock b1_mutex
        pthread_mutex_unlock(&b1_mutex);

        // Increment b2_line_count
        b2_line_count++;

        // Replace last character line feed with a space if it is not the end marker
        if (strcmp(buffer1[i], END_MARKER)) {
            buffer2[i][strlen(buffer2[i]) - 1] = ' ';
        }
        else {

            /* End critical section for buffer2, signal plusSignThread that buffer2
            has new data and unlock b2_mutex */ 
            pthread_cond_signal(&b2_full);
            pthread_mutex_unlock(&b2_mutex);
            break;
        }

        /* End critical section for buffer2, signal plusSignThread that buffer2
        has new data and unlock b2_mutex */    
        pthread_cond_signal(&b2_full);
        pthread_mutex_unlock(&b2_mutex);

        i++;
    }  

    return NULL;
}

/*
* Replace all "++" with "^"
*/
void* plusSign(void* args) {

    // Initialize left and right pointers for scanning input text
    int b2_l_ptr = 0;
    int b2_r_ptr = 1;

    /* Initialize loop i for buffer2 index, b3_indx for buffer3 index,
    oop through buffer2 until END_MARKER is reached*/
    int i = 0; 
    int b3_indx = 0; 
    while (1) {
        
        /* Critical section start for buffer2, lock b2_mutex before
        checking if buffer2 has new data or adjusting b2_line_count. This
        way it will pause for us until new data is added in buffer2. */
        pthread_mutex_lock(&b2_mutex);
        while (b2_line_count == 0) {
            pthread_cond_wait(&b2_full, &b2_mutex);
        }

        /* Critical section start for buffer3, lock b3_mutex before
        checking if buffer2[i] containes the end marker and inserting
        \n into buffer3 as a replacement, and adjusting b3_char_count */
        pthread_mutex_lock(&b3_mutex);
        if (strcmp(buffer2[i], END_MARKER) == 0) {
            buffer3[b3_indx] = '\n';
            b3_char_count++;

            /* End critical area for buffer3, signal outputThread buffer3 
            has new data, unlock b3_mutex */
            pthread_cond_signal(&b3_full);
            pthread_mutex_unlock(&b3_mutex);
            break;
        }
        pthread_mutex_unlock(&b3_mutex);

        /* We know there is new data in buffer2 now, it is safe to temporarily 
        unlock b2_mutex until we need to adjust shared memory related to b2_mutex. 
        This way b2 is not locked for an arbitrary amount of input characters */
        pthread_mutex_unlock(&b2_mutex);

        // Copy each character current index of buffer2 into buffer3 one at a time
        while (buffer2[i][b2_l_ptr] != '\0') {

            /* Critical section start for buffer3, lock b3_mutex before
            inserting data and adjusting b3_char_count */
            pthread_mutex_lock(&b3_mutex);

            // Are the b2 pointers looking at "++"?
            if (buffer2[i][b2_l_ptr] == '+' && buffer2[i][b2_r_ptr] == '+') {
                
                // Yes, insert a single "^" character and iterate b2 l and r pointers.
                buffer3[b3_indx] = '^';
                b2_l_ptr++;
                b2_r_ptr++;
            } else {

                // No, copy buffer2 left character into buffer 3
                buffer3[b3_indx] = buffer2[i][b2_l_ptr]; 
            }

            // Iterate b3 character count
            b3_char_count++;

            /* End critical area for buffer3, signal outputThread buffer3 
            has new data, unlock b3_mutex */
            pthread_cond_signal(&b3_full);
            pthread_mutex_unlock(&b3_mutex);

            // Iterate buffer2 pointers and b3 index outside of critical section
            b2_l_ptr++;
            b2_r_ptr++;
            b3_indx++;
        }
        
        /* Critical section of buffer2 start again. Lock b2_mutex
        before decrementing b2_line_count */
        pthread_mutex_lock(&b2_mutex);
        b2_line_count--;

        // End of critical section for buffer2, unlock b2_mutex
        pthread_mutex_unlock(&b2_mutex);

        // Increment b2 l and r pointers and index i outside of critical section
        b2_l_ptr = 0;
        b2_r_ptr = 1;
        i++;
    }
   
    return NULL;
}

/*
* Only print when 80 characters are available to print. Print
* exactly 80 characters each time.
*/
void* output(void* args) {

    // Initialize print buffer and allocate memory for 80 characters
    int print_size = 80;
    char* print_buffer;
    print_buffer = calloc(print_size + 1, sizeof(char));

    int b3_indx = 0; 
    while(1) {

        /* Start critical section for buffer3, lock b3_mutex before checking
        if buffer3 has any data */
        pthread_mutex_lock(&b3_mutex);
        while ((b3_char_count < print_size && buffer3[strlen(buffer3) - 1] != '\n')) {
            pthread_cond_wait(&b3_full, &b3_mutex);
        }

        // Break out of while loop when there is not enough characters remaining to print
        if (b3_char_count < print_size + 1 && buffer3[strlen(buffer3) - 1] == '\n') {
            break;
        }

        strncpy(print_buffer, &buffer3[b3_indx], print_size);
        b3_char_count -= print_size;

        // End of critical section for buffer3, unlock b3_mutex
        pthread_mutex_unlock(&b3_mutex);

        // Print 80 characters to the screen
        printf(print_buffer);
        printf("\n");
        b3_indx += print_size;
    }

    // Free dynamic memory used for print buffer
    free(print_buffer);
    return NULL;
}

/*
* Create 4 serperate threads to concurrently edit and print different aspects 
* of user input in an organized fashion.
*/
int main(void) {

    // Initiate threads
    pthread_t inputThread, lineSeparatorThread, plusSignThread, outputThread;

    // Create threads with their respective functions
    pthread_create(&inputThread, NULL, input, NULL);
    pthread_create(&lineSeparatorThread, NULL, lineSeparator, NULL);
    pthread_create(&plusSignThread, NULL, plusSign, NULL);
    pthread_create(&outputThread, NULL, output, NULL);

    // Wait for threads to complete
    pthread_join(inputThread, NULL);
    pthread_join(lineSeparatorThread, NULL);
    pthread_join(plusSignThread, NULL);
    pthread_join(outputThread, NULL); 
}
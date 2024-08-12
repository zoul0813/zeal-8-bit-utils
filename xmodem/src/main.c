/* SPDX-FileCopyrightText: 2023 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "zos_errors.h"
#include "zos_vfs.h"
#include "zos_sys.h"
#include "zos_serial.h"


// + 1 for NUL terminator
#define BUFFER_SIZE 128

#define NUL     0x00
#define SOH     0x01
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18
#define SUB     0x1A

static zos_err_t receive(const char* filename);
static zos_err_t send(const char* filename);
static void print(const char* buffer, int size);

static void print(const char* buffer, int size) {
    for(int i = 0; i < size; ++i) {
        const char c = buffer[i];
        switch(c) {
            case 0x09:
                printf("  ");
                break;
            default:
                putchar(c);
        }
    }
}

static zos_err_t read_char(zos_dev_t dev, const char* c) {
    uint16_t size = 1;
    zos_err_t err = ERR_FAILURE;

    err = read(dev, c, &size);
    // printf("read: %d == %d %d\n", c, buffer[0], err);

    if(err != ERR_SUCCESS) {
        printf("Error reading device: %d", err);
        return err;
    }
    return ERR_SUCCESS;
}

static zos_err_t wait_for(zos_dev_t dev, const char c) {
    char buffer[1];
    uint16_t size = 1;
    zos_err_t err = ERR_FAILURE;

    uint8_t orig_blocking = 0;

    int attempts = 16;
    while(attempts > 0) {
        err = read_char(dev, buffer);

        if(buffer[0] == c) {
            return ERR_SUCCESS;
        }
        --attempts;
    }

    return ERR_FAILURE;
}

static zos_err_t receive(const char* filename) {
    printf("Receive: %s\n", filename);
    zos_dev_t uart = open("#SER0", O_RDWR);
    /* Check if it was a success, abort else */
    if (uart < 0) {
        printf("Could not open #SER0");
        return ERR_INVALID_FILEDEV;
    }

    zos_err_t err = ERR_SUCCESS;

    char buffer[BUFFER_SIZE];
    while(1) {
        int size = BUFFER_SIZE - 1;
        err = read(uart, buffer, &size);
        if(err != ERR_SUCCESS) {
            printf("Error reading file: %d\n", size);
            return err;
        } else if(size == 0) {
            break;
        }
        buffer[size] = 0;
        printf("%s", buffer);
    }

    /* Close the opened directory */
    close(uart);
    return ERR_SUCCESS;
}

static zos_err_t send(const char* filename) {
    zos_dev_t file = open(filename, O_RDONLY);
    if (file < 0) {
        printf("Could not open '%s'\n", filename);
        return -file;
    }

    zos_dev_t uart = open("#SER0", O_RDWR);
    if (uart < 0) {
        printf("Could not open #SER0");
        return -uart;
    }
    printf("Ready, start the download\n");

    zos_err_t err = ERR_SUCCESS;
    // wait for the initial NAK to begin transmitting
    err = wait_for(uart, NAK);
    if(err != ERR_SUCCESS) {
        printf("Receiving failed to acknowledge: %d\n", err);
        return err;
    }

    printf("Acknowledged, sending data\n");
    /**
     * Transmit
    */
    char buffer[BUFFER_SIZE];
    uint8_t block = 0;
    uint16_t bytes_sent = 0;
    uint16_t filesize = 0;
    uint16_t _written = 3;
    while(1) {
        uint16_t size = 1;
        uint16_t writeSize = 1;
        write(DEV_STDOUT, ".", &writeSize);
        size = BUFFER_SIZE;
        err = read(file, buffer, &size);
        if(err != ERR_SUCCESS) {
            printf("Error reading file: %d\n", size);
            return err;
        }
        filesize += size;

        const char header[3] = { SOH, ++block, ~block };
        current_block:
        _written = 3;
        err = write(uart, header, &_written);
        if(err != ERR_SUCCESS) {
            printf("UART Error [SOH]: %d", err);
            break;
        }
        bytes_sent += _written;
        char checksum = 0;
        for(uint8_t i = 0; i < BUFFER_SIZE; ++i) {
            char b[1] = { SUB };
            if(i < size) {
                b[0] = buffer[i];
                checksum += buffer[i];
                ++bytes_sent;
            }
            _written = 1;
            err = write(uart, b, &_written);
            if(err != ERR_SUCCESS) {
                printf("UART Error [Data]: %d", err);
                break;
            }
        }

        const char tail[1] = { checksum % 256 };
        _written = 1;
        err = write(uart, tail, &_written);
        if(err != ERR_SUCCESS) {
            printf("UART Error: %d", err);
            break;
        }
        bytes_sent += _written;

        if(size < BUFFER_SIZE-1) {
            break;
        }

        if(block % 40 == 0) printf("\n");

        while(1) {
            const char acknak[1];
            err = read_char(uart, acknak);
            if(err != ERR_SUCCESS) {
                printf("Error reading char: %d", err);
                return err;
            }
            switch(acknak[0]) {
                case ACK:
                    // printf("R: ACK\n");
                    goto next_block;
                case NAK:
                    writeSize = 1;
                    write(DEV_STDOUT, "N", &writeSize);
                    goto current_block;
                case CAN:
                    writeSize = 1;
                    write(DEV_STDOUT, "C", &writeSize);
                    goto end_transmission;
                default:
                    writeSize = 1;
                    write(DEV_STDOUT, "X", &writeSize);
                    // printf("R: %d\n", acknak[0]);
            }
        }
        next_block:
    }

    /**
     * End Transmission
    */
    msleep(200); // wait 200ms, instead of looking for ACK/NAK

    while(1) {
        buffer[0] = EOT;
        _written = 1;
        err = write(uart, buffer, &_written);
        if(err != ERR_SUCCESS) {
            printf("UART Error [EOT]: %d", err);
        }

        const char acknak[1];
        err = read_char(uart, acknak);
        if(err != ERR_SUCCESS) {
            printf("Error reading char: %d", err);
            return err;
        }
        if(acknak[0] == ACK) break;
    }

    end_transmission:
    close(file);
    close(uart);
    printf("\nEnd of Transmission:\n");
    printf("Filesize: %d\n", filesize);
    printf("Sent %d bytes in %d blocks\n", bytes_sent, block);
    printf("\n\n");
    return ERR_SUCCESS;
}

void print_usage(void) {
    printf("Usage: xm.bin [s,r] [filename]\n\n");
    printf("Example:\n");
    printf("  xm.bin s file.txt\n\n");
}

int main(int argc, char** argv) {
    zos_err_t ret = 0;

    if (argc == 1) {
        const char* args[2] = { NUL, NUL };
        const char* token = strtok(argv[0], " ");
        uint8_t tokens = 0;
        while(token != NUL) {
            args[tokens] = token;
            token = strtok(NUL, " ");
            ++tokens;
        }
        if(tokens != 2) {
            print_usage();
            return ERR_INVALID_PARAMETER;
        }

        const char* filename = args[1];
        const char* mode = args[0];

        switch(mode[0]) {
            case 'r':
                printf("Receiving: %s\n", filename);
                ret = receive(filename);
                break;
            case 's':
                printf("Sending: %s\n", filename);
                ret = send(filename);
                break;
        }
    } else {
        print_usage();
        return ERR_INVALID_PARAMETER;
    }

    if(ret != ERR_SUCCESS) {
        printf("\n\nERROR: %d\n", ret);
        exit(1);
    }

    return ret;
}

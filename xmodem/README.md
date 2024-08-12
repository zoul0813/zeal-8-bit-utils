# XMODEM for Zeal 8-bit OS

This directory contains an XMODEM implementation for Zeal 8-bit OS.

## How to compile

In order to compile the example, you will need SDCC v4.4.0 or above.

Then, to compile this example, you will only need the following command:

```shell
make
```

The output should look like this:

```shell
rm -fr bin/
mkdir -p bin
sdcc -mz80 -c --codeseg TEXT -I~/Zeal-8-bit-OS/kernel_headers/sdcc/include/ -o bin/./ src/main.c
sdldz80 -n -mjwx -i -b _HEADER=0x4000  -k ~/Zeal-8-bit-OS/kernel_headers/sdcc/lib -l z80 bin/xm.ihx ~/Zeal-8-bit-OS/kernel_headers/sdcc/bin/zos_crt0.rel bin/main.rel
gobjcopy --input-target=ihex --output-target=binary bin/xm.ihx bin/xm.bin
Success, binary generated: bin/xm.bin
uartrcv 5544 xm.bin
```

As stated on the final line, the binary that can be transferred and executed by Zeal 8-bit OS is `bin/xm.bin`.

## How to use

Once loaded in Zeal 8-bit OS, the program can be executed with `exec xm.bin` or `./xm.bin`

If run without any arguments, it will display the usage message

```text
Usage: xm.bin [s,r] [filename]

Example:
  xm.bin s file.txt
```

The first argument is either an `r` or `s` for receive or send, respectively.

The second argument is the path to the file you wish to transmit.

## Cleaning the binaries

Two possibilities to clean the resulted binaries:

* Delete the binary folder:

    ```shell
    rm -r bin
    ```

* Use make:

    ```shell
    make clean
    ```


## References

The following links may be useful if attempting to write an XMODEM Implementation on your own.

* [XMODEM Packet Structure](https://en.wikipedia.org/wiki/XMODEM#Packet_structure)
* [XMODEM File Transfers](https://web.mit.edu/6.121/www/other/pcplot_man/pcplot14.htm)
* [XMODEM Example in C](https://www.menie.org/georges/embedded/xmodem.html)
* [XMODEM Example in Python](https://stackoverflow.com/questions/358471/xmodem-for-python)

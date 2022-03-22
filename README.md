# yaus
yaus is a toy implementation of a unix shell. It is mainly written for fun and no profit.

## Usage
Use `gcc -o yaus shell.c` to compile. Then use `./yaus` to execute.

## Features
- Simple commands e.g `ls -l`
- IO redirections e.g `cat < shell.c > bar.c`
- Pipes e.g `cat < shell.c | grep int | cat  > result.txt`
- Builtins (`cd` and `exit`)
- Asynchronous execution e.g `cat < shell.c &` 

## ToDo
- Add support for Signals
- Add support for quoting, globbing and environment variables

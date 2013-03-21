CFLAGS = -Wall
pasvmim: ftp.o main.o
	$(CC) -o pasvmim ftp.o main.o
clean:
	rm -f pasvmim ftp.o main.o

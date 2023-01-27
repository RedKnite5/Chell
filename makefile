
sshell: sshell.c
	gcc -Wall -Wextra sshell.c -o sshell

clean:
	rm -f sshell *.txt

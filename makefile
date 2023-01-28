
sshell: sshell.c
	gcc -o2 -Wall -Wextra -Werror sshell.c -o sshell

clean:
	rm -f sshell *.txt

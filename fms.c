#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <sys/wait.h>

void solicita_dados(char *binario, int *cpu_quota, int *timeout, long *mem_max) {
    char buffer[256];

    printf("Digite o comando a ser executado: ");
    fgets(binario, 256, stdin);
    binario[strcspn(binario, "\n")] = 0; // remove \n do final

    printf("Digite o tempo máximo de CPU (em segundos): ");
    fgets(buffer, sizeof(buffer), stdin);
    sscanf(buffer, "%d", cpu_quota);

    printf("Digite o tempo máximo de execução (em segundos): ");
    fgets(buffer, sizeof(buffer), stdin);
    sscanf(buffer, "%d", timeout);

    printf("Digite o limite máximo de memória (em KB): ");
    fgets(buffer, sizeof(buffer), stdin);
    sscanf(buffer, "%ld", mem_max);
}

long monitor(int pid, int timeout, int cpu_quota, long mem_max) {
    struct rusage usage;
    int status;
    int tempo_espera = 0;

    // verifica a cada 1 segundo se o processo terminou até atingir o timeout
    while (tempo_espera < timeout) {
        int result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            break;
        }

        sleep(1);
        tempo_espera++;
    }

    // se o tempo estourou, mata o processo
    if (tempo_espera >= timeout) {
        printf("Tempo limite atingido! Matando o processo...\n");
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0); // garantir que foi coletado
    }

    // coleta dados de uso de CPU e memória
    getrusage(RUSAGE_CHILDREN, &usage);

    long cpu_user = usage.ru_utime.tv_sec;
    long cpu_sys  = usage.ru_stime.tv_sec;
    long cpu_total = cpu_user + cpu_sys;

    printf("Tempo de CPU (usuário + sistema): %ld s\n", cpu_total);

    if (cpu_total > cpu_quota) {
        printf("Limite de CPU excedido!\n");
        printf("Encerrando FMS.\n");
        exit(0);
    }

    long mem_used = usage.ru_maxrss; // em KB no Linux
    printf("Memória máxima usada: %ld KB\n", mem_used);

    if (mem_used > mem_max) {
        printf("Limite de memória excedido!\n");
        printf("Encerrando FMS.\n");
        exit(0);
    }
    return cpu_total;
}


int main() {
    char binario[256];
    int cpu_quota;        // em segundos
    int timeout;          // em segundos
    long mem_max;         // em KB
    long cpu_restante;

    while (1) {
        solicita_dados(binario, &cpu_quota, &timeout, &mem_max);
        cpu_restante = cpu_quota;

        int pid = fork();

        if (pid == 0) {
            // Processo filho: executa o binário
            char *argv[2];
            argv[0] = binario;
            argv[1] = NULL;
            execvp(binario, argv);
            printf("Erro ao executar o binário");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Processo pai: monitora
            long cpu_usado = monitor(pid, timeout, cpu_quota, mem_max);
            cpu_restante -= cpu_usado;
            if (cpu_restante <= 0)
            {
                printf("Quota de CPU esgotada. Encerrando FMS.\n");
                break;
            }
        } else {
            printf("Erro no fork");
            exit(EXIT_FAILURE);
        }
        char continuar;
        char buffer[10];
        printf("Deseja executar outro programa? (s/n): ");
        fgets(buffer, sizeof(buffer), stdin);
        sscanf(buffer, " %c", &continuar);
    }

    return 0;
}
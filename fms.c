#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

typedef struct {
    int cpu_quota, cpu_usado, timeout;
    long mem_max, mem_usada;
} Recursos;

void solicitar_limites(Recursos *r) {
    printf("Quota de CPU (s): "); scanf("%d", &r->cpu_quota);
    printf("Timeout por execução (s): "); scanf("%d", &r->timeout);
    printf("Memória máxima (KB): "); scanf("%ld", &r->mem_max);
    r->cpu_usado = 0; r->mem_usada = 0;
}

int monitorar(int pid, Recursos *r) {
    struct rusage uso_ini, uso_fim;
    int status;
    time_t inicio;
    
    getrusage(RUSAGE_CHILDREN, &uso_ini);
    time(&inicio);
    
    while (1) {
        if (waitpid(pid, &status, WNOHANG) == pid) break;
        if (time(NULL) - inicio >= r->timeout) {
            printf("Timeout expirado! Matando processo...\n");
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        sleep(1);
    }

    getrusage(RUSAGE_CHILDREN, &uso_fim);

    // calcula CPU usando rusage (usuario + sistema)
    double cpu = (uso_fim.ru_utime.tv_sec - uso_ini.ru_utime.tv_sec)
               + (uso_fim.ru_stime.tv_sec - uso_ini.ru_stime.tv_sec)
               + (uso_fim.ru_utime.tv_usec - uso_ini.ru_utime.tv_usec) / 1e6
               + (uso_fim.ru_stime.tv_usec - uso_ini.ru_stime.tv_usec) / 1e6;

    long mem = uso_fim.ru_maxrss;

    printf("Processo atual: CPU usado: %.2fs | Memória usada: %ld KB\n", cpu, mem);
    
    // Só registra recursos se o binário executou (não falhou no execvp)
    if ((WIFEXITED(status) && WEXITSTATUS(status) != 127) || WIFSIGNALED(status)) {

        r->cpu_usado += (int)cpu; 
        if (mem > r->mem_usada) r->mem_usada = mem;
        
        printf("Acumulado: CPU: %d/%d s (%.1f%%) | Memória: %ld/%ld KB (%.1f%%)\n", 
               r->cpu_usado, r->cpu_quota, 
               (r->cpu_quota > 0) ? (r->cpu_usado * 100.0 / r->cpu_quota) : 0,
               r->mem_usada, r->mem_max, 
               (r->mem_max > 0) ? (r->mem_usada * 100.0 / r->mem_max) : 0);
    
        
        if (r->cpu_usado > r->cpu_quota) {
            printf("Quota de CPU excedida!\n");
            return 0;
        }
        if (r->mem_usada > r->mem_max) {
            printf("Limite de memória excedido!\n");
            return 0;
        }
    } else {
        printf("Execução falhou - recursos não descontados\n");
    }

    return 1;
}

int executar(char *cmd, Recursos *r) {
    if (cmd[0] == '\0') return 1;
    
    int pid = fork();
    if (pid < 0) {
        perror("Erro no fork");
        return 0;
    } else if (pid == 0) {
        char *args[] = {cmd, NULL};
        execvp(cmd, args);
        perror("Erro ao executar");
        exit(127);
    }
    return monitorar(pid, r);
}

int main() {
    char cmd[256];
    Recursos r;

    printf("=== FURG METERED SHELL (FMS) ===\n");
    solicitar_limites(&r);

    while (1) {
        printf("\nDigite o comando (ou 'sair'): ");
        scanf(" %[^\n]", cmd);

        if (strcmp(cmd, "sair") == 0) break;
        if (!executar(cmd, &r)) break;
    }

    printf("=== FIM DO FMS ===\n");
    return 0;
}
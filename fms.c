#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>

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
    int status, tempo = 0;

    getrusage(RUSAGE_CHILDREN, &uso_ini);
    while (tempo++ < r->timeout) {
        if (waitpid(pid, &status, WNOHANG) == pid) break;
        sleep(1);
    }

    if (tempo > r->timeout) {
        printf("Timeout expirado! Matando processo...\n");
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }

    getrusage(RUSAGE_CHILDREN, &uso_fim);

    double cpu = (uso_fim.ru_utime.tv_sec - uso_ini.ru_utime.tv_sec)
               + (uso_fim.ru_stime.tv_sec - uso_ini.ru_stime.tv_sec)
               + (uso_fim.ru_utime.tv_usec - uso_ini.ru_utime.tv_usec) / 1e6
               + (uso_fim.ru_stime.tv_usec - uso_ini.ru_stime.tv_usec) / 1e6;

    long mem = uso_fim.ru_maxrss;

    printf("CPU: %.2fs | Memória: %ld KB\n", cpu, mem);

    r->cpu_usado += (int)cpu;
    if (mem > r->mem_usada) r->mem_usada = mem;

    if (r->cpu_usado > r->cpu_quota) {
        printf("Quota de CPU excedida!\n");
        return 0;
    }

    if (r->mem_usada > r->mem_max) {
        printf("Limite de memória excedido!\n");
        return 0;
    }

    return 1;
}

int executar(char *bin, Recursos *r) {
    int pid = fork();
    if (pid < 0) {
        perror("Erro no fork");
        return 0;
    } else if (pid == 0) {
        execlp(bin, bin, NULL);
        perror("Erro ao executar");
        exit(1);
    }
    return monitorar(pid, r);
}

int main() {
    char bin[256];
    Recursos r;

    printf("=== FURG METERED SHELL (FMS) ===\n");
    solicitar_limites(&r);

    while (1) {
        printf("\nDigite o binário (ou 'sair'): ");
        scanf(" %[^\n]", bin);

        if (strcmp(bin, "sair") == 0) break;
        if (!executar(bin, &r)) break;

        printf("CPU restante: %d s | Memória restante: %ld KB\n",
            r.cpu_quota - r.cpu_usado,
            (r.mem_max > r.mem_usada) ? r.mem_max - r.mem_usada : 0);
    }

    printf("=== FIM DO FMS ===\n");
    return 0;
}

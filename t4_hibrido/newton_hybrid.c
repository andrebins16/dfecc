#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include <time.h>
#include <mpi.h>
#include <omp.h>

#define LARGURA_BASE 2000
#define ALTURA_BASE 2000
#define MAX_ITERACOES 1000
#define EPSILON 1e-6

#define X_MIN -0.05
#define X_MAX  0.05
#define Y_MIN -0.05
#define Y_MAX  0.05

#define TAG_TRABALHO 1
#define TAG_RESULTADO 2
#define TAG_TERMINO 3

void salvar_matriz(int **matriz, double tempo_execucao, int largura, int altura, const char *arquivo_saida) {
    FILE *fp = fopen(arquivo_saida, "w");
    if (!fp) {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    fprintf(fp, "%d %d %.4f %.17f %.17f %.17f %.17f\n",
            largura, altura, tempo_execucao, X_MIN, X_MAX, Y_MIN, Y_MAX);

    for (int y = 0; y < altura; y++) {
        for (int x = 0; x < largura; x++) {
            fprintf(fp, "%d", matriz[y][x]);
            if (x < largura - 1) fprintf(fp, " ");
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

int calcula_convergencia(complex double z) {
    for (int i = 0; i < MAX_ITERACOES; i++) {
        complex double f = cpow(z, 3) - 1;
        complex double f_linha = 3 * cpow(z, 2);
        if (cabs(f) < EPSILON) return i;
        z = z - f / f_linha;
    }
    return MAX_ITERACOES;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <multiplicador_de_trabalho> <num_threads_openmp>\n", argv[0]);
        return 1;
    }

    int multiplicador_trabalho = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    if (multiplicador_trabalho <= 0 || num_threads <= 0) {
        fprintf(stderr, "Parametros inválidos. Devem ser maiores que 0.\n");
        return 1;
    }

    int largura = LARGURA_BASE * multiplicador_trabalho;
    int altura = ALTURA_BASE;

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    //MESTRE
    if (rank == 0) {
        printf("Execução paralela com %d processos pesados MPI, com %d threads OpenMP por processo. \n Total de processos = %d\n Multiplicador de trabalho = %d\n",
               size, num_threads, (size-1)*num_threads, multiplicador_trabalho);

        int **matriz = malloc(altura * sizeof(int *));
        for (int i = 0; i < altura; i++)
            matriz[i] = malloc(largura * sizeof(int));

        int linha_atual = 0;
        int ativos = 0;
        double inicio = MPI_Wtime();

        for (int i = 1; i < size && linha_atual < altura; i++) {
            MPI_Send(&linha_atual, 1, MPI_INT, i, TAG_TRABALHO, MPI_COMM_WORLD);
            linha_atual++;
            ativos++;
        }

        while (ativos > 0) {
            int *resultado = malloc((largura + 1) * sizeof(int));
            MPI_Status status;
            MPI_Recv(resultado, largura + 1, MPI_INT, MPI_ANY_SOURCE, TAG_RESULTADO, MPI_COMM_WORLD, &status);

            int linha_id = resultado[0];
            for (int j = 0; j < largura; j++)
                matriz[linha_id][j] = resultado[j + 1];
            free(resultado);

            if (linha_atual < altura) {
                MPI_Send(&linha_atual, 1, MPI_INT, status.MPI_SOURCE, TAG_TRABALHO, MPI_COMM_WORLD);
                linha_atual++;
            } else {
                MPI_Send(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_TERMINO, MPI_COMM_WORLD);
                ativos--;
            }
        }

        double fim = MPI_Wtime();
        double tempo = fim - inicio;

        char nome_arquivo[100];
        snprintf(nome_arquivo, sizeof(nome_arquivo), "newton_%dcoresMpi_%dthreadsOmp_mult%d_output.dat",
                 size, num_threads, multiplicador_trabalho);

        salvar_matriz(matriz, tempo, largura, altura, nome_arquivo);

        for (int i = 0; i < altura; i++) free(matriz[i]);
        free(matriz);

        printf("Tempo de execução: %.4f segundos\n", tempo);
    } 
    //ESCRAVO
    else {
        omp_set_num_threads(num_threads);  // define o número de threads
        //laço principal do escravo
        while (1) {
            int linha;
            MPI_Status status;

            MPI_Recv(&linha, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == TAG_TERMINO) break;

            int *resultado = malloc((largura + 1) * sizeof(int));
            resultado[0] = linha;

            double variavel_imaginaria = Y_MIN + (Y_MAX - Y_MIN) * linha / (altura - 1);

            #pragma omp parallel for schedule(dynamic) // paralelismo com omp. atribuição dinamica
            for (int x = 0; x < largura; x++) {
                double variavel_real = X_MIN + (X_MAX - X_MIN) * x / (largura - 1);
                complex double z = variavel_real + variavel_imaginaria * I;
                resultado[x + 1] = calcula_convergencia(z);
            }

            MPI_Send(resultado, largura + 1, MPI_INT, 0, TAG_RESULTADO, MPI_COMM_WORLD);
            free(resultado);
        }
    }

    MPI_Finalize();
    return 0;
}

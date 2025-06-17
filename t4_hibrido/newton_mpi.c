#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

#define LARGURA_BASE 2000
#define ALTURA_BASE 2000
#define MAX_ITERACOES 1000
#define EPSILON 1e-6

#define X_MIN -0.05
#define X_MAX  0.05
#define Y_MIN -0.05
#define Y_MAX  0.05

#define TAG_TRABALHO 1 //tag para o mestre enviar o trabalho
#define TAG_RESULTADO 2 //tag para o escravo enviar o resultado
#define TAG_TERMINO 3 //tag para mestre indicar que o saco esvaziou

//salva a matriz no arquivo especificado
void salvar_matriz(int **matriz, double tempo_execucao, int largura, int altura, const char *arquivo_saida) {
    FILE *fp = fopen(arquivo_saida, "w");
    if (!fp) {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    //Cabeçalho
    fprintf(fp, "%d %d %.4f %.17f %.17f %.17f %.17f\n",
            largura, altura, tempo_execucao, X_MIN, X_MAX, Y_MIN, Y_MAX);

    //Matriz
    for (int y = 0; y < altura; y++) {
        for (int x = 0; x < largura; x++) {
            fprintf(fp, "%d", matriz[y][x]);
            if (x < largura - 1) fprintf(fp, " ");
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

//calcula quantas iterações o ponto z leva para convergir a uma raíz de f(z) = z³ - 1
int calcula_convergencia(complex double z) {
    for (int i = 0; i < MAX_ITERACOES; i++) {
        complex double f = cpow(z, 3) - 1;  //calcula a função
        complex double f_linha = 3 * cpow(z, 2); //calcula derivada
        if (cabs(f) < EPSILON) return i; //convergiu (encontrou uma raíz), então retorna o número de iterações
        z = z - f / f_linha; // próximo passo segundo o método de Newton-Raphson --> x_(n+1) = x_n - f(x_n) / f'(x_n)
    }
    return MAX_ITERACOES; // não convergiu
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <multiplicador_de_trabalho>\n", argv[0]);
        return 1;
    }

    //Multiplicador do tamanho do problema (escalabilidade fraca)
    int multiplicador_trabalho = atoi(argv[1]);
    if (multiplicador_trabalho <= 0) {
        fprintf(stderr, "Valor inválido para multiplicador de trabalho.\n");
        return 1;
    }

    int largura = LARGURA_BASE * multiplicador_trabalho; // para aumentar o problema na escalabilidade fraca
    int altura = ALTURA_BASE;

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    //MESTRE
    if (rank == 0) {
        printf("Execução paralela com %d cores e com multiplicador de cores = %d\n", size, multiplicador_trabalho);

        //matriz de resultados
        int **matriz = malloc(altura * sizeof(int *));
        for (int i = 0; i < altura; i++)
            matriz[i] = malloc(largura * sizeof(int));

        int linha_atual = 0; // linha a ser enviada
        int ativos = 0; // numero de escravos que ainda estao trabalhando
        double inicio = MPI_Wtime();

        //envia 1 linha para cada escravo
        for (int i = 1; i < size && linha_atual < altura; i++) {
            MPI_Send(&linha_atual, 1, MPI_INT, i, TAG_TRABALHO, MPI_COMM_WORLD); // envia só o índice da linha
            linha_atual++;
            ativos++;
        }

        //laço principal do mestre
        while (ativos > 0) {
            int *resultado = malloc((largura + 1) * sizeof(int)); //array para o resultado do trabalho de um escravo
            MPI_Status status;
            MPI_Recv(resultado, largura + 1, MPI_INT, MPI_ANY_SOURCE, TAG_RESULTADO, MPI_COMM_WORLD, &status); //recebe o resultado de qualquer escravo

            int linha_id = resultado[0]; // na primeira posição está qual linha que foi executada 

            for (int j = 0; j < largura; j++) 
                matriz[linha_id][j] = resultado[j + 1]; //coloca o resultado na linha correta. (j+1) por conta do primeiro indice "morto"

            free(resultado);

            if (linha_atual < altura) { //ainda tem trabalho no saco
                MPI_Send(&linha_atual, 1, MPI_INT, status.MPI_SOURCE, TAG_TRABALHO, MPI_COMM_WORLD); //envia o índice da próxima linha
                linha_atual++;
            } else {
                MPI_Send(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_TERMINO, MPI_COMM_WORLD); //acabou o saco, envia sinal de termino
                ativos--;
            }
        }

        double fim = MPI_Wtime();
        double tempo = fim - inicio;
        char nome_arquivo[100];
        snprintf(nome_arquivo, sizeof(nome_arquivo), "newton_%dcores_parallel_mult%d_output.dat", size, multiplicador_trabalho); //cria o nome do arquivo de saída dinamicamente
        salvar_matriz(matriz, tempo, largura, altura, nome_arquivo); //salva a matriz no arquivo
        for (int i = 0; i < altura; i++) free(matriz[i]);
        free(matriz);

        printf("Tempo de execução: %.4f segundos\n", tempo);
    }
    //ESCRAVO    
    else {
        //laço principal do escravo
        while (1) {
            int linha;
            MPI_Status status;

            MPI_Recv(&linha, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status); // recebe o índice da linha
            if (status.MPI_TAG == TAG_TERMINO) break; // recebeu sinal de termino, logo acaba

            //senão, é um sinal de trabalho

            int *resultado = malloc((largura + 1) * sizeof(int)); //cria o array de resultado
            resultado[0] = linha; //já indica qual a linha que foi executada na resposta 
            
            double variavel_imaginaria = Y_MIN + (Y_MAX - Y_MIN) * linha / (altura - 1);
            //faz o trabalho na linha, calculando a convergencia
            for (int x = 0; x < largura; x++) {
                double variavel_real = X_MIN + (X_MAX - X_MIN) * x / (largura - 1);
                complex double z = variavel_real + variavel_imaginaria * I; //transforma o ponto em um numero complexo dentro da área determinada 
                resultado[x + 1] = calcula_convergencia(z); //calcula a convergencia
            }

            MPI_Send(resultado, largura + 1, MPI_INT, 0, TAG_RESULTADO, MPI_COMM_WORLD); //envia o resultado de volta
            free(resultado);
        }
    }

    MPI_Finalize();
    return 0;
}

# T2Paralelos - Fractal de Newton

## Programa sequencial: newton_seq
# Compilação: gcc -o newton_seq newton_seq.c -lm
# Execução: ./newton_seq <multiplicador_de_trabalho>

## Programa paralelo: newton_mpi
# Compilação: mpicc -o newton_mpi newton_mpi.c -lm
# Execução: mpirun -np <numero_de_nucleos> ./newton_mpi <multiplicador_de_trabalho>

## Programa para gerar gráficos
# Execução: python script_imagem_newton.py <arquivo_dados>
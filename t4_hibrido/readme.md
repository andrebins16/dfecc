# T4Paralelos - Fractal de Newton

## Programa paralelo: newton_mpi
# Compilação: mpicc -o newton_mpi newton_mpi.c -lm
# Execução: mpirun -np <numero_de_nucleos> ./newton_mpi <multiplicador_de_trabalho>

## Programa paralelo: newton_hybrid
# Compilação: mpicc -fopenmp -o newton_hybrid newton_hybrid.c -lm
# Execução: mpirun -np 5 ./newton_hybrid <multiplicador_de_trabalho> <num_threads_openmp>

## Programa para gerar gráficos
# Execução: python script_imagem_newton.py <arquivo_dados>
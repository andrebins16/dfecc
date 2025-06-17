import numpy as np
import matplotlib.pyplot as plt
import sys
import os

if len(sys.argv) != 2:
    print(f"Uso: python {sys.argv[0]} <arquivo.dat>")
    sys.exit(1)

arquivo_dat = sys.argv[1]

# pega o nome da imagem a ser criada a partir do nome do arquivo
nome_base = os.path.splitext(os.path.basename(arquivo_dat))[0]
arquivo_png = nome_base + ".png"

# le arquivo
with open(arquivo_dat, "r") as f:
    header = f.readline().strip().split()
    WIDTH = int(header[0])
    HEIGHT = int(header[1])
    TEMPO = float(header[2])
    X_MIN = float(header[3])
    X_MAX = float(header[4])
    Y_MIN = float(header[5])
    Y_MAX = float(header[6])

    data = np.genfromtxt(f, dtype=int) #pega os dados


# gera a imagem
plt.figure(figsize=(8, 8))
plt.imshow(data, cmap="twilight_shifted", extent=[X_MIN, X_MAX, Y_MIN, Y_MAX])
plt.title(f"Fractal de Newton - {WIDTH}x{HEIGHT} - Tempo: {TEMPO:.2f}s")
plt.axis("off")
plt.tight_layout()
plt.savefig(arquivo_png, dpi=150)
plt.show()

# Simple Contour Extractor

Ferramenta que extrai bordas de uma imagem bitmap simples e as traduz para SVG.

Pipeline:

    Imagem BMP/PNG
      -> (C++, stb_image)      matriz em escala de cinza -> CSV
      -> (Fortran, Canny)      contornos ordenados       -> JSON (schema próprio)
      -> (C++, RDP + path "L") SVG final (sem curvas de Bézier)

## Requisitos

- GCC 16 (fornece `g++` e `gfortran`)
- CMake >= 3.28
- Python 3.13+
- stb_image.h

## Build

    mkdir build && cd build
    cmake ..
    cmake --build .

## Executar o pipeline

    python3 run.py caminho/para/imagem.bmp saida.svg

## Decisões de projeto

- Detecção de borda: Canny completo (suavização, NMS, hysteresis, edge linking), não apenas Sobel.
- Formato intermediário de contornos: JSON de schema fixo e específico. Não há parser JSON genérico em nenhum dos lados (Fortran ou C++).
- Geração do SVG: RDP para simplificar os pontos, seguido de path com apenas comandos `L` (linha). Sem ajuste de curvas de Bézier.
- Sem testes automatizados nesta etapa inicial.

## Situação atual do projeto

- Em desenvolvimento!

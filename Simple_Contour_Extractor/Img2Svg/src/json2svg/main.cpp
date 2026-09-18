// Etapa 3 (C++): Base para o SVG
//
// Responsabilidade: ler o JSON de contornos, simplificar cada contorno com Ramer-Douglas-Peucker
// e escrever um SVG final usando SOMENTE comandos de linha ("L") no path, sem ajuste de curvas de Bezier.

#include <vector>

struct Point { double x; double y; };
struct Contour { int id; std::vector<Point> points; };

auto main(int argc, char** argv) -> int {

    // TODO: ler argumentos de entrada;
    // TODO: ler json_path e popular largura, altura e vector<Contour>
    //       usando o parser ad-hoc para o schema de docs/json-schema.md.
    // TODO: para cada Contour, aplicar Ramer-Douglas-Peucker (epsilon
    //       configuravel) para reduzir o numero de pontos.
    // TODO: montar o path SVG usando apenas comandos M (move) e L (line),
    //       sem C/Q (Bezier) -- decisao de escopo do projeto.
    // TODO: escrever o arquivo SVG final em svg_path.

    return 1;
}

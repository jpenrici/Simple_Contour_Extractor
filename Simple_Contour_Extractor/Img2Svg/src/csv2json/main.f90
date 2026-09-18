! Etapa 2 (Fortran): Tratamento Numérico
!
! Responsabilidade: motor matemático isolado. Lê o CSV gerado pelo C++
! como matriz numérica pura e aplica o pipeline de Canny completo:
!   1) suavização gaussiana
!   2) gradiente (Sobel 3x3, magnitude e direção)
!   3) non-maximum suppression
!   4) hysteresis thresholding
!   5) ligação/rastreamento de bordas (edge linking) para produzir
!      contornos ORDENADOS (nao apenas pixels de borda isolados)
!
! Saida: JSON de schema fixo e específico não é um serializador JSON genérico.

program csv2json
    implicit none

    character(len=256) :: csv_path, json_path

    if (command_argument_count() /= 2) then
        write (0, *) "use: csv2json <input_csv> <output_json>"
        stop 1
    end if

    call get_command_argument(1, csv_path)
    call get_command_argument(2, json_path)

    ! TODO: ler csv_path como matriz numérica (linha = pixel row).
    ! TODO: aplicar suavização gaussiana.
    ! TODO: aplicar Sobel 3x3 (Gx, Gy) -> magnitude e direção do gradiente.
    ! TODO: non-maximum suppression sobre a magnitude.
    ! TODO: hysteresis thresholding (limiares alto/baixo) -> pixels de borda.
    ! TODO: ligar pixels de borda em contornos ordenados (edge linking).
    ! TODO: escrever json_path seguindo o schema especifico do projeto.

    write (0, *) "csv2json: not yet implemented (", trim(csv_path), &
        " -> ", trim(json_path), ")"
    stop 1
end program edges

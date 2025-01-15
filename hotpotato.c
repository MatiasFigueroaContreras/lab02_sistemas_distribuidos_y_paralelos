#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int FLAG_IS_OUT = 1;   // Flag que indica que un proceso ya no está en juego.
int FLAG_END_GAME = 2; // Flag que indica que un proceso es el ganador.
int FLAG_CONTINUE = 0; // Flag que indica que el juego continúa.
int FLAG_CONTINUE_DO_NOT_PRINT = -1; // Flag que indica indica que el juego continúa y que no se debe imprimir el estado del juego.

// Función que calcula el siguiente proceso que jugará en base
//  a los procesos que aún están en juego.
int calculate_next(int myrank, int numprocs, int *processors_playing)
{
    int next = (myrank + 1) % numprocs;
    while (processors_playing[next] == 0)
    {
        next = (next + 1) % numprocs;
    }
    return next;
}

// Función que envía mensajes a todos los procesos que aún están en juego,
// ademas se incluye el proceso 0 el cual es el encargado de imprimir
// el estado del juego.
void custom_bcast(int *processors_playing, int *msg, int numprocs, MPI_Comm comm)
{
    for (int i = 0; i < numprocs; i++)
    {
        if (i != msg[0] && (processors_playing[i] == 1 || i == 0))
        {
            // Se envía el mensaje a todos los procesos que aún están en juego y al proceso 0.
            // asegurando que el mensaje fue recepcionado y asi todos estan enterados de la salida
            // del proceso antes de continuar con el juego (por lo que se utiliza MPI_Ssend).
            MPI_Ssend(msg, 4, MPI_INT, i, 1, comm);
        }
    }
}

// Función que imprime el estado del juego en base al flag que recibe.
void print_game_status(int myrank, int token, int next, int flag)
{
    if (flag == FLAG_IS_OUT)
    {
        printf("Proceso %d tiene la papa con valor %d (proceso %d sale del juego)\n", myrank, token, myrank);
    }
    else if (flag == FLAG_END_GAME)
    {
        printf("Proceso %d es el ganador\n", myrank);
    }
    else
    {
        printf("Proceso %d tiene la papa con valor %d\n", myrank, token);
    }
}

int main(int argc, char *argv[])
{
    // Se obtienen los argumentos de entrada.
    int opt;
    int TOKEN_INIT;
    int M;
    while ((opt = getopt(argc, argv, "t:M:")) != -1)
    {
        switch (opt)
        {
        case 't':
            TOKEN_INIT = atoi(optarg);
            break;
        case 'M':
            M = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Uso: %s -t [Token inicial] -M [Valor maximo - 1, para el numero aleatorio]\n", argv[0]);
            exit(EXIT_FAILURE);
            abort();
        }
    }

    int myrank, numprocs;
    // Se inicializa el proceso que comienza el juego (proceso 0).
    int playing = 0;
    int token = TOKEN_INIT;
    int next = 0;
    // Formato de mensaje (msg): {myrank, token, next, flag}
    int msg[4];
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    // Se inicializa el arreglo que indica si un proceso ya no está en juego.
    // teniendo como 1 el proceso que aún está en juego y
    // 0 el proceso que no está en juego.
    int processors_playing[numprocs];
    for (int i = 0; i < numprocs; i++)
    {
        processors_playing[i] = 1;
    }
    // Se inicializa la semilla para los números aleatorios
    //  (se le suma myrank para que no se repita la secuencia de
    //   numeros aletarorios entre los distintos procesos).
    srand(time(NULL) + myrank);

    // Cada procesador comienza el juego esperando su turno.
    while (1)
    {
        if (playing == myrank)
        {
            next = calculate_next(myrank, numprocs, processors_playing);

            // Si el siguiente proceso es el mismo, entonces el proceso actual
            // es el ganador.
            if (next == myrank)
            {
                if (myrank == 0)
                {
                    print_game_status(myrank, token, next, FLAG_END_GAME);
                }
                else
                {
                    int msg_winner[4] = {myrank, token, next, FLAG_END_GAME};
                    MPI_Send(msg_winner, 4, MPI_INT, 0, 0, MPI_COMM_WORLD);
                }
                break;
            }
            playing = next;

            // Se actualiza el token restando un numero aleatorio entre 0 y M - 1.
            token = token - (rand() % M);
            if (token < 0)
            {
                int msg_bcast[4] = {myrank, token, next, FLAG_IS_OUT};
                if (myrank == 0)
                {
                    print_game_status(myrank, token, next, FLAG_IS_OUT);
                    processors_playing[myrank] = 0;
                }

                // Se envía el mensaje comunicando el proceso que salio
                //  a todos los procesos que aún están en juego.
                custom_bcast(processors_playing, msg_bcast, numprocs, MPI_COMM_WORLD);
                // Se envia mensaje con el token al proceso que debe continuar el juego.
                msg_bcast[3] = FLAG_CONTINUE_DO_NOT_PRINT;
                MPI_Send(msg_bcast, 4, MPI_INT, next, 0, MPI_COMM_WORLD);
                if (myrank != 0){
                    break;
                }
            }
            else
            {
                if (myrank == 0)
                    print_game_status(myrank, token, next, 0);
                int msg_bcast[4] = {myrank, token, next, FLAG_CONTINUE};
                if (myrank != 0 && next != 0){
                    // Se envía el mensaje al proceso 0 para que imprima el estado del juego.
                    //  con MPI_Ssend para asegurar que el mensaje fue recepcionado.
                    MPI_Ssend(msg_bcast, 4, MPI_INT, 0, 0, MPI_COMM_WORLD);
                }
                // Se envía el mensaje con el token al proceso que debe continuar el juego.
                MPI_Send(msg_bcast, 4, MPI_INT, next, 0, MPI_COMM_WORLD);
            }
        }
        else
        {
            MPI_Recv(&msg, 4, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (myrank == 0 && msg[3] != FLAG_CONTINUE_DO_NOT_PRINT)
            {
                // Proceso 0 encargado de imprimir el estado del juego.
                print_game_status(msg[0], msg[1], msg[2], msg[3]);
            }

            if (msg[3] == FLAG_IS_OUT)
            {
                // Se actualiza el arreglo de procesos que aún están en juego.
                processors_playing[msg[0]] = 0;
            }
            else if (msg[3] == FLAG_END_GAME)
            {
                break;
            }
            else {
                // Se actualiza el token y el proceso que debe jugar.
                token = msg[1];
                playing = msg[2];
                if (token < 0)
                {
                    token = TOKEN_INIT;
                }
            }
        }
    }
    MPI_Finalize();
    return 0;
}
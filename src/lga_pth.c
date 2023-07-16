#include "lga_base.h"
#include "lga_pth.h"

#include "pthread_barrier.h"
#include <pthread.h>
#include <stdlib.h>

static byte get_next_cell(int i, int j, byte *grid_in, int grid_size) {
    byte next_cell = EMPTY;

    for (int dir = 0; dir < NUM_DIRECTIONS; dir++) {
        int rev_dir = (dir + NUM_DIRECTIONS/2) % NUM_DIRECTIONS;
        byte rev_dir_mask = 0x01 << rev_dir;

        int di = directions[i%2][dir][0];
        int dj = directions[i%2][dir][1];
        int n_i = i + di;
        int n_j = j + dj;

        if (inbounds(n_i, n_j, grid_size)) {
            if (grid_in[ind2d(n_i,n_j)] == WALL) {
                next_cell |= from_wall_collision(i, j, grid_in, grid_size, dir);
            }
            else if (grid_in[ind2d(n_i, n_j)] & rev_dir_mask) {
                next_cell |= rev_dir_mask;
            }
        }
    }

    return check_particles_collision(next_cell);
}

static void update(byte *grid_in, byte *grid_out, int grid_size, int i_start, int i_end, int j_start, int j_end) {
    for (int i = i_start; i <= i_end; i++) {
        int line_start = (i == i_start ? j_start : 0);
        int line_end = (i == i_end ? j_end : grid_size - 1);
        for (int j = line_start; j <= line_end ; j++) {
            if (grid_in[ind2d(i,j)] == WALL)
                grid_out[ind2d(i,j)] = WALL;
            else
                grid_out[ind2d(i,j)] = get_next_cell(i, j, grid_in, grid_size);
        }
    }
}

struct uniform_info {
    byte *grid_1;
    byte *grid_2;
    int grid_size;
    pthread_barrier_t barrier;
};

struct thread_info {
    struct uniform_info *uniform;
    int start_ind;
    int end_ind;
};

void* thread_routine(void* arg) {
    struct thread_info *info = arg;

    int i_start = info->start_ind / info->uniform->grid_size;
    int j_start = info->start_ind % info->uniform->grid_size;

    int i_end = (info->end_ind - 1) / info->uniform->grid_size;
    int j_end = (info->end_ind - 1) % info->uniform->grid_size;

    for (int i = 0; i < ITERATIONS/2; i++) {
        update(
            info->uniform->grid_1,
            info->uniform->grid_2,
            info->uniform->grid_size,
            i_start,
            i_end,
            j_start,
            j_end
        );
        pthread_barrier_wait(&info->uniform->barrier);
        update(
            info->uniform->grid_2,
            info->uniform->grid_1,
            info->uniform->grid_size,
            i_start,
            i_end,
            j_start,
            j_end
        );
        pthread_barrier_wait(&info->uniform->barrier);
    }

    return NULL;
}

void simulate_pth(byte *grid_1, byte *grid_2, int grid_size, int num_threads) {
    if (num_threads == 0) return;

    struct uniform_info *uniform = malloc(sizeof(struct uniform_info));

    uniform->grid_1 = grid_1;
    uniform->grid_2 = grid_2;
    uniform->grid_size = grid_size;
    pthread_barrier_init(&uniform->barrier, NULL, num_threads);

    struct thread_info *thr_info = malloc(num_threads * sizeof(struct thread_info));
    pthread_t *thr = malloc((num_threads - 1) * sizeof(pthread_t));

    int size = grid_size * grid_size;
    int last_end = 0;

    for (int i = 0; i < num_threads; i++) {
        thr_info[i].uniform = uniform;
        thr_info[i].start_ind = last_end;
        thr_info[i].end_ind = last_end + size / num_threads + (i < size % num_threads ? 1 : 0);
        last_end = thr_info[i].end_ind;

        if (i + 1 != num_threads) {
            pthread_create(&(thr[i]), NULL, thread_routine, (void*)(&(thr_info[i])));
        }
    }

    thread_routine(&(thr_info[num_threads-1]));

    for (int i = 0; i < num_threads - 1; i++) {
        pthread_join(thr[i], NULL);
    }

    free(thr);
    free(thr_info);
    pthread_barrier_destroy(&uniform->barrier);
    free(uniform);
}

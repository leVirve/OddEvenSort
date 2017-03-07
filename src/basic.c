#include <mpi.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _DEBUG
#define DEBUG(format, args...) printf("[Line:%d] " format, __LINE__, ##args);
#else
#define DEBUG(args...)
#endif
#ifdef _INFO
#define INFO(format, args...) printf("[Line:%d] " format, __LINE__, ##args);
#else
#define INFO(args...)
#endif

#define bool        char
#define true        1
#define false       0
#define channel1    0
#define channel2    1
#define EVEN_PHASE  0
#define ODD_PHASE   1

#define is_odd(x)   ((x) & 1)
#define is_even(x)  (!is_odd(x))
#define swap(i, j)  int t = i; i = j; j = t;
#define timeit(t)   clock_gettime(CLOCK_REALTIME, t)

bool sorted = false;
int world_size, world_rank, subset_size;

long io_time = 0, comp_time = 0, comm_time = 0;
struct timespec s, e;

void calc_time(long* target, struct timespec a, struct timespec b)
{
    int sec = a.tv_sec - b.tv_sec;
    int nsec = a.tv_nsec - b.tv_nsec;
    *target += sec * 1000000000 + nsec;
}

void mpi_read_file(char* filename, int* nums, int* count)
{
    MPI_File input;
    MPI_Status status;
    timeit(&s);
    MPI_File_open(MPI_COMM_WORLD, filename,
                  MPI_MODE_RDONLY, MPI_INFO_NULL, &input);
    MPI_File_set_view(input, sizeof(int) * subset_size * world_rank,
                      MPI_INT, MPI_INT, "native", MPI_INFO_NULL);
    MPI_File_read_all(input, nums, subset_size, MPI_INT, &status);
    MPI_File_close(&input);
    timeit(&e); calc_time(&io_time, e, s);
    MPI_Get_count(&status, MPI_INT, count);
    DEBUG("My rank=%d/%d read_size=%d\n", world_rank, world_size, *count);
}

void mpi_write_file(char* filename, int* nums, int* count)
{
    MPI_File fh;
    MPI_Status status;
    timeit(&s);
    MPI_File_open(MPI_COMM_WORLD, filename,
                  MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &fh);
    MPI_File_set_view(fh, sizeof(int) * subset_size * world_rank,
                      MPI_INT, MPI_INT, "native", MPI_INFO_NULL);
    MPI_File_write_all(fh, nums, *count, MPI_INT, &status);
    MPI_File_close(&fh);
    timeit(&e); calc_time(&io_time, e, s);
}

void mpi_recv(int rank, int* nums)
{
    if (world_rank >= world_size || rank < 0) return;
    int carrier;
    MPI_Status status;
    timeit(&s);
    MPI_Recv(&carrier, 1, MPI_INT, rank, channel1, MPI_COMM_WORLD, &status);
    timeit(&e); calc_time(&comm_time, e, s);
    timeit(&s);
    if (carrier > nums[0]) { swap(carrier, nums[0]); sorted = false; }
    timeit(&e); calc_time(&comp_time, e, s);
    timeit(&s);
    MPI_Send(&carrier, 1, MPI_INT, rank, channel2, MPI_COMM_WORLD);
    timeit(&e); calc_time(&comm_time, e, s);
}

void mpi_send(int rank, int* nums, int count)
{
    if (rank >= world_size) return;
    int carrier;
    MPI_Status status;
    timeit(&s);
    MPI_Send(&nums[count - 1], 1, MPI_INT, rank, channel1, MPI_COMM_WORLD);
    MPI_Recv(&carrier, 1, MPI_INT, rank, channel2, MPI_COMM_WORLD, &status);
    timeit(&e); calc_time(&comm_time, e, s);
    nums[count - 1] = carrier;
}

void _single_phase_sort(int* a, int index, int size)
{
    int i = index;
    timeit(&s);
    for (; i < size - 1; i += 2)
        if (a[i] > a[i + 1]) { swap(a[i], a[i + 1]); sorted = false; }
    timeit(&e); calc_time(&comp_time, e, s);
}

int main(int argc, char** argv)
{
    int file_size;
    sscanf(argv[1], "%d", &file_size);

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    bool single_process = false;
    /** For data size is less then processors **/
    if (file_size < world_size) world_size = file_size;
    if (world_size <= 1) single_process = true;

    subset_size = file_size / world_size;
    if (file_size % world_size) subset_size += 1;

    int count, *nums = (int*) malloc(subset_size * sizeof(int));
    int head = subset_size * world_rank, tail = head + subset_size - 1;
    mpi_read_file(argv[2], nums, &count);

    DEBUG("#%d/%d count=%d\n", world_rank, world_size, count);
#ifdef _TIME
    printf("world_size %d\n", world_size);
#endif
    while (!sorted) {
        sorted = true;

        /*** even-phase ***/
        if (!single_process) {
            if (is_even(world_rank) && is_even(tail) ||
                is_even(world_rank) && is_odd(head))
                mpi_send(world_rank + 1, nums, count);
            if (is_odd(world_rank) && is_odd(head) ||
                is_odd(world_rank) && is_even(tail))
                mpi_recv(world_rank - 1, nums);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        _single_phase_sort(nums, EVEN_PHASE, count);

        /*** odd-phase ***/
        if (!single_process) {
            if (is_even(world_rank) && is_odd(tail) ||
                is_odd(world_rank) && is_odd(tail))
                mpi_send(world_rank + 1, nums, count);
            if (is_odd(world_rank) && is_even(head) ||
                is_even(world_rank) && is_even(head))
                mpi_recv(world_rank - 1, nums);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        _single_phase_sort(nums, ODD_PHASE, count);

        if (!single_process) {
            bool tmp = sorted;
            timeit(&s);
            MPI_Allreduce(&tmp, &sorted, 1, MPI_CHAR, MPI_BAND, MPI_COMM_WORLD);
            timeit(&e); calc_time(&comm_time, e, s);
        }
    }
    mpi_write_file(argv[3], nums, &count);
    free(nums);
    DEBUG("#%d leave sorting-loop(%d)\n", world_rank, count);
    MPI_Finalize();

#ifdef _TIME
    printf("#Rank%d: total=%ld nsec (io=%ld, comm=%ld, comp=%ld)\n",
           world_rank,
           io_time + comm_time + comp_time,
           io_time, comm_time, comp_time);
#endif

    return 0;
}

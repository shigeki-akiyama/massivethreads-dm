#include <uth.h>
#include <madm_comm.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/time.h>

#include <tuple>
using namespace std;
namespace uth = madm::uth;


static size_t busy(size_t loops)
{
    if (loops > 100) {
        long t = uth::tick();
        while (uth::tick() - t < (long)loops)
            uth::poll();
    }

    return 0;
}

static size_t bin(size_t depth, size_t leaf_loops, size_t interm_loops,
                  size_t interm_iters)
{
    size_t i;

    if (depth == 0) {
        busy(leaf_loops);
        return 1;
    } else {
        size_t result = 1;
        for (i = 0; i < interm_iters; i++) {
            uth::thread<size_t> f([=] {
                return bin(depth - 1, leaf_loops, interm_loops,
                           interm_iters);
            });

            result += bin(depth - 1, leaf_loops,
                          interm_loops, interm_iters);

            result += f.join();

            busy(interm_loops);
        }

        return result;
    }
}

void real_main(int argc, char **argv)
{
    const size_t n_args = 5;
    if (argc != n_args + 1) {
        fprintf(stderr,
                "usage: %s depth leaf_loops interm_loops interm_iters "
                "pre_exec\n",
                argv[0]);
        return;
    }

    size_t arg_idx = 1;
    size_t depth = (size_t)atol(argv[arg_idx++]);
    size_t leaf_loops = (size_t)atol(argv[arg_idx++]);
    size_t interm_loops = (size_t)atol(argv[arg_idx++]);
    size_t interm_iters = (size_t)atol(argv[arg_idx++]);
    size_t pre_exec = (size_t)atol(argv[arg_idx++]);
    assert(arg_idx == n_args + 1);

    uth::pid_t me = uth::get_pid();
    size_t n_procs = uth::get_n_procs();

    if (me == 0) {
        printf("program = bin,\n");
        printf("depth = %zu, leaf_loops = %zu, "
               "interm_loops = %zu, interm_iters = %zu, pre_exec = %zu\n",
               depth, leaf_loops,
               interm_loops, interm_iters, pre_exec);
        uth::print_options(stdout);
    }

    uth::barrier();

    // pre-exec
    size_t i;
    for (i = 0; i < pre_exec; i++) {
        uth::barrier();

        if (me == 0) {
            uth::thread<size_t> f([=] {
                return bin(depth, leaf_loops, interm_loops, interm_iters);
            });
            f.join();
        }
    }

    uth::barrier();

    if (0 && me == 0) {
       fprintf(stderr, "------------------- START ------------------------\n");
       fflush(stderr);
    }
    
    // do main execution
    double t0 = uth::time();

    size_t nodes = 0;
    if (me == 0) {
        uth::thread<size_t> f([=] {
            return bin(depth, leaf_loops, interm_loops, interm_iters);
        });
        nodes = f.join();
    }

    double t1 = uth::time();
    
    uth::barrier();

    if (0 && me == 0) {
        fprintf(stderr, "-------------------- END -----------------------\n");
        fflush(stderr);
    }
    uth::barrier();
    
    if (me == 0) {
        double time = t1 - t0;
        double throughput = (double)nodes / time;

        size_t server_mod = madi::comm::get_server_mod();
        
        printf("np = %zd, server_mod = %zu, time = %.6f,\n"
               "throughput = %.6f, throughput/np = %.6f, "
               "task overhead = %.0f\n",
               n_procs, server_mod, time,
               1e-6 * throughput,
               1e-6 * throughput / (double)n_procs,
               1e+9 / throughput * (double)n_procs);
    }
    uth::barrier();
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}


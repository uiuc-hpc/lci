# Choose the right PMI backend
Choosing the right PMI backend used to be important. However, after the upgrade of 
cmake variable `<LCT|LCI>_PMI_BACKEND_DEFAULT` and environment variable `LCT_PMI_BACKEND` into a list
of options for LCI to try. The default value can handle most of the cases so users don't need
to care too much about it anymore. Nevertheless, we keep the old documents here in case users
want to manually decide which PMI backend to use.

LCI offers five PMI backends: pmi1,
pmi2, pmix, mpi, and local. You can use `<LCT|LCI>_PMI_BACKEND_DEFAULT` to specify the default
backend when running `cmake`, or the environment variable `LCT_PMI_BACKEND` to specify
the backend when executing the program.

An easy way to decide the right PMI backend is to run
```
mpirun -n 1 env | grep PMI
```
if you are using `mpirun`, or
```
srun -n 1 env | grep PMI
```
if you are using `srun`. If you see `PMI_RANK` in the output, you should use either pmi1
or pmi2. If you see `PMIX_RANK` in the output, you should use pmix.

If you are using `srun` but can see neither `PMI_RANK` nor `PMIX_RANK`, you should try
the `--mpi` arguments of `srun`. For example,
```
srun --mpi=pmi2 -n 1 ./application
```
should set up a pmi2 environment.
```
srun --mpi=pmix -n 1 ./application
```
should set up a pmix environment.

As a last resort, you can always use the `mpi` backend.
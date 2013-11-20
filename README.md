1. make sure the Makefile points to the local installation of nvcc. (Already set up
   for 302.

2. run "make"

3. run "mpirun -mca btl_tcp_if_include eth0 -H <list of hosts (302x15,302x14,...)> 
   vector_add <binary_file_a> <binary_file_b>"

4. output file will be hist.c

5. diff as desired

6. to run with text files instead of binary files, switch the comments on the 4
   indicated lines in the vector_add.cpp file and make again


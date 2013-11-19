/* Simple example demonstrating how to use MPI with CUDA
 *
 *  Generate some random numbers on one node.
 *  Dispatch them to all nodes.
 *  Compute their square root on each node's GPU.
 *  Compute the average of the results using MPI.
 *
 */

#include <iostream>
#include <sys/stat.h>
#include <stdlib.h>
#include <mpi.h>

#include "vector_add.h"

using std::cout;
using std::cerr;
using std::endl;

#define MPI_CHECK(call) \
   if((call) != MPI_SUCCESS) { \
      cerr << "MPI error calling \""#call"\"\n"; \
      exit(-1); };


int main(int argc, char *argv[])
{
   // Initialize MPI state
   MPI_CHECK(MPI_Init(&argc, &argv));

   // Get our MPI node number and node count
   int commSize, commRank;
   MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &commSize));
   MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &commRank));

   int i = 0;
   int j = 0;
   int offset = 0;
   int size_node = 0;

   float *vector_a = NULL;
   float *vector_b = NULL;

   int *vector_bins = NULL;
   int *histogram_node = NULL;
   int *histogram_gather = NULL;
   int *histogram = NULL;
   int *histogram_recv = NULL;
   int *histogram_final = NULL;



   int size_a = size_file(argv[1]);
   int size_b = size_file(argv[2]);

   if(commRank == 0) {

      if(size_a != size_b) {
         cerr << "Input files must contain vectors of the same size" << endl;
         MPI_CHECK(MPI_Finalize());
         exit(-1);
      }

      if(size_a < commSize) {
         cerr << "There really isn't a need to parallelize such a small file across so many machines!" << endl;
         MPI_CHECK(MPI_Finalize());
         exit(-1);
      }

      size_node = size_a / commSize + (size_a % commSize);
      offset = 0;

   } else {

      size_node = size_a / commSize;
      offset = (size_a / commSize + (size_a % commSize)) + ((size_a / commSize) * (commRank-1));
   }


   if(NULL == (vector_a = (float *)malloc(size_node * sizeof(float)))) {
      MPI_Finalize();
      exit(-1);
   }

   if(NULL == (vector_b = (float *)malloc(size_node * sizeof(float)))) {
      MPI_Finalize();
      exit(-1);
   }

   //vector_file_binary(argv[1], vector_a, size_node, offset);
   //vector_file_binary(argv[2], vector_b, size_node, offset);
   vector_file(argv[1], vector_a, size_node, offset);
   vector_file(argv[2], vector_b, size_node, offset);

   if(NULL == (vector_bins = (int *)malloc(size_node * sizeof(int)))) {
      MPI_Finalize();
      exit(-1);
   }

   computeAddGPU(vector_a, vector_b, vector_bins, size_node);

   if(NULL == (histogram_node = (int *)malloc(80 * sizeof(int)))) {
      MPI_Finalize();
      exit(-1);
   }
   memset(histogram_node, 0, 80 * sizeof(int));
   for(i = 0; i < size_node; i++) {
      histogram_node[(vector_bins[i] > 79 ? 79 : vector_bins[i])]++;
   }

   if(commRank != 0) {
      MPI_CHECK(MPI_Send(histogram_node, 80, MPI_INT, 0, 0, MPI_COMM_WORLD));
   } else {
      for(j = 1; j < commSize; j++) {

         if(NULL == (histogram_recv = (int *)malloc(80 * sizeof(int)))) {
            MPI_Finalize();
            exit(-1);
         }
         MPI_CHECK(MPI_Recv(histogram_recv, 80, MPI_INT, j, 0, MPI_COMM_WORLD, NULL));

         for(i = 0; i < 80; i++) {
            histogram_node[i] += histogram_recv[i];
         }

         free(histogram_recv);
      }

      FILE *fp = fopen("hist.c", "w");
      for(j = 0; j < 80; j++) {
         fprintf(fp, "%d, %d\n", j, histogram_node[j]);
      }
      fclose(fp);
   }

   MPI_CHECK(MPI_Finalize());
   return 0;
}

int size_file(char *filename) {

   struct stat stbuf;
   int flag = stat(filename, &stbuf);
   int filesize = stbuf.st_size;

   FILE *fp = fopen(filename, "r");

   char *text;
   if(NULL == (text = (char *)malloc((filesize+1) * sizeof(char)))) {
      MPI_Finalize();
      exit(-1);
   }
   int nchar = fread(text, sizeof(char), filesize, fp);
   text[nchar] = '\0';

   char whitespace[7] = " \t\n\f\r\0";
   char *word = strtok(text, whitespace);

   int i = 0;
   while(word) {
      word = strtok(NULL, whitespace);
      i++;
   }

   free(text);

   fclose(fp);

   return i;
}

int vector_file(char *filename, float *vector, int size, int offset) {

   struct stat stbuf;
   int flag = stat(filename, &stbuf);
   int filesize = stbuf.st_size;

   FILE *fp = fopen(filename, "r");

   char *text;
   if(NULL == (text = (char *)malloc((filesize+1) * sizeof(char)))) {
      MPI_Finalize();
      exit(-1);
   }
   int nchar = fread(text, sizeof(char), filesize, fp);
   text[nchar] = '\0';

   char whitespace[7] = " \t\n\f\r\0";
   char *word = strtok(text, whitespace);

   int i;
   for(i = 0; word; i++) {

      if(i >= offset) {
         if(i >= offset+size) {
            break;
         }

         vector[i-offset] = atof(word);
      }
      word = strtok(NULL, whitespace);
   }

   free(text);
   fclose(fp);

   return 0;

}

int size_file_binary(char *filename) {

   FILE *fp = fopen(filename, "r");
   int size;

   fread(&size, sizeof(int), 1, fp);
   fclose(fp);

   return size;
}

int vector_file_binary(char *filename, float *vector, int size, int offset) {

   FILE  *fp = fopen(filename, "r");

   fseek(fp, (offset + 1) * sizeof(int), SEEK_SET); //seek to my portion

   fread(vector, sizeof(float), size, fp);

   fclose(fp);

   return 0;
}

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

#define NUM_BINS 80
#define HIST_MIN -20.0
#define HIST_MAX 20.0

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

   //UNCOMMENT THESE LINES FOR TEXT FILE READING
   //int size_a = size_file(argv[1]);
   //int size_b = size_file(argv[2]);

   //UNCOMMENT THESE LINES FOR BINARY FILE READING
   int size_a = size_file_binary(argv[1]);
   int size_b = size_file_binary(argv[2]);

   if(commRank == 0) {

      //root compares sizes of input files
      if(size_a != size_b) {
         cerr << "Input files must contain vectors of the same size" << endl;
         MPI_CHECK(MPI_Finalize());
         exit(-1);
      }

      //weird results if we use too many machines on a small file
      if(size_a < commSize) {
         cerr << "There really isn't a need to parallelize such a small file across so many machines!" << endl;
         MPI_CHECK(MPI_Finalize());
         exit(-1);
      }

      //size of root node includes the possible modulus in addition to its divided share
      size_node = size_a / commSize + (size_a % commSize);

      //roots offset begins at one
      offset = 0;

   } else {

      //node vector size
      size_node = size_a / commSize;

      //nodes begin their offset starting at the end of the roots share
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

   //the next functions read the vector of size_node at offset into the specified float pointer

   //COMMENT THESE LINES FOR BINARY FILE READING
   vector_file_binary(argv[1], vector_a, size_node, offset);
   vector_file_binary(argv[2], vector_b, size_node, offset);
   
   //UNCOMMENT THESE LINES FOR TEXT FILE READING
   //vector_file(argv[1], vector_a, size_node, offset);
   //vector_file(argv[2], vector_b, size_node, offset);


   if(NULL == (vector_bins = (int *)malloc(size_node * sizeof(int)))) {
      MPI_Finalize();
      exit(-1);
   }

   //this CUDA function combines the vectors into a vector of the same size indicating which bin
   //this added result should be counted towards
   computeAddGPU(vector_a, vector_b, vector_bins, size_node);

   if(NULL == (histogram_node = (int *)malloc(NUM_BINS * sizeof(int)))) {
      MPI_Finalize();
      exit(-1);
   }

   //histogram_node represents each nodes histogram of the added and 'binned' initial vectors
   memset(histogram_node, 0, NUM_BINS * sizeof(int));
   for(i = 0; i < size_node; i++) {

      //the 'binning' in the CUDA code will result in 0-NUM_BINS bins but we want 0-NUM_BINS where the
      //largest numbers are in bin NUM_BINS-1
      histogram_node[(vector_bins[i] > NUM_BINS-1 ? NUM_BINS-1 : vector_bins[i])]++;
   }


   if(commRank != 0) {
      //if we are a node, send our histogram to the root
      MPI_CHECK(MPI_Send(histogram_node, NUM_BINS, MPI_INT, 0, 0, MPI_COMM_WORLD));

   } else {
      //if we are the root, accept everyones histogram and add it to our own
      for(j = 1; j < commSize; j++) {

         if(NULL == (histogram_recv = (int *)malloc(NUM_BINS * sizeof(int)))) {
            MPI_Finalize();
            exit(-1);
         }
         MPI_CHECK(MPI_Recv(histogram_recv, NUM_BINS, MPI_INT, j, 0, MPI_COMM_WORLD, NULL));

         for(i = 0; i < NUM_BINS; i++) {
            histogram_node[i] += histogram_recv[i];
         }

         free(histogram_recv);
      }

      //write the resultant histogram to a file
      FILE *fp = fopen("hist.c", "w");
      for(j = 0; j < NUM_BINS; j++) {
         fprintf(fp, "%d, %d\n", j, histogram_node[j]);
      }
      fclose(fp);
   }

   free(vector_a);
   free(vector_b);
   free(vector_bins);
   free(histogram_node);
   free(histogram_gather);
   free(histogram);
   free(histogram_final);

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

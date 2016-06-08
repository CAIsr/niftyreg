
#include "_reg_ReadWriteImage.h"
#include "_reg_ReadWriteBinary.h"
#include "_reg_mrf.h"
#include "_reg_localTrans.h"
#include "_reg_resampling.h"
#include "_reg_mind.h"

#define EPS 0.000001

int main(int argc, char **argv)
{
   time_t start;
   time(&start);

   if(argc!=4) {
      fprintf(stderr, "Usage: %s <indexNeighbours> <edgeWeightMatrix> <expectedParentsList>\n", argv[0]);
      return EXIT_FAILURE;
   }
   //IO
   char *indexNeighboursName=argv[1];
   char *edgeWeightMatrixName=argv[2];
   char *expectedParentsListName=argv[3];
   //char *expectedOrderedListName=argv[4];
   //char *expectedEdgeWeightName=argv[5];

   //CP - cf. matlab test
   int nb_CP = 30;
   int nbEdges = nb_CP*6;

   int* indexNeighbours = new int[nbEdges];
   readIntBinaryArray(indexNeighboursName, nbEdges, indexNeighbours);
   float* edgeWeightMatrix = new float[nbEdges];
   readFloatBinaryArray(edgeWeightMatrixName, nbEdges, edgeWeightMatrix);
   //
   //int* expectedOrderedList = new int[nb_CP];
   //readIntBinaryArray(expectedOrderedListName, nb_CP, expectedOrderedList);
   int* expectedParentsList = new int[nb_CP];
   readIntBinaryArray(expectedParentsListName, nb_CP, expectedParentsList);
   //float* expectedEdgeWeight = new float[nb_CP];
   //readFloatBinaryArray(expectedEdgeWeightName, nb_CP, expectedEdgeWeight);

   reg_mrf* reg_mrfObject =
           new reg_mrf(2,1,3,nb_CP);

   reg_mrfObject->GetPrimsMST(edgeWeightMatrix,indexNeighbours, nb_CP, 6, false);
   //COMPARE THE RESULTS
   int* olP = reg_mrfObject->GetOrderedListPtr();
   int* plP = reg_mrfObject->GetParentsListPtr();
   float* ewP = reg_mrfObject->GetEdgeWeightPtr();
   //
   for(size_t i=0;i<nb_CP;i++) {
       //DEBUG
       //std::cout<<"olP[i]="<<olP[i]+1<<std::endl;
       //std::cout<<"plP[i]+1="<<plP[i]+1<<std::endl;
       //std::cout<<"expectedParentsList[i]="<<expectedParentsList[i]<<std::endl;
       //std::cout<<"ewP[i]="<<ewP[i]<<std::endl;
       //DEBUG
       //if((olP[i]+1 - expectedOrderedList[i]) != 0) {
       //    reg_print_msg_error("the 2 MST are differents");
       //    return EXIT_FAILURE;
       //}
       if((plP[i]+1 - expectedParentsList[i]) != 0) {
           reg_print_msg_error("the 2 MST are differents");
           return EXIT_FAILURE;
       }
       //if(std::abs(ewP[i] - expectedEdgeWeight[i]) > EPS) {
       //    reg_print_msg_error("the 2 MST are differents");
       //    return EXIT_FAILURE;
       //}
   }
   //
   delete[] indexNeighbours;
   delete[] edgeWeightMatrix;

   //delete[] expectedOrderedList;
   delete[] expectedParentsList;
   //delete[] expectedEdgeWeight;
   //
#ifndef NDEBUG
   printf("All good\n");
#endif
   return EXIT_SUCCESS;
}

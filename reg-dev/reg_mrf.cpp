#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include "_reg_ReadWriteImage.h"
#include "_reg_localTrans.h"
#include "_reg_localTrans_jac.h"
#include "_reg_ssd.h"
#include <numeric>

#include "_reg_mind.h"
#include "_reg_mrf.h"
#include "_reg_ReadWriteMatrix.h"


int main(int argc, char **argv)
{
    time_t start;
    time(&start);

    if(argc!=6) {
        fprintf(stderr, "Usage: %s <refImage> <floatingImage> <regularisationWeight> <outputImageName> <initFile>\n", argv[0]);
        return EXIT_FAILURE;
    }
    //IO
    char *inputRefImageName=argv[1];
    char *inputFloImageName=argv[2];
    float regularisationWeight=atof(argv[3]);
    char *outputImageName=argv[4];
    char *affineTransformationName=argv[5];

    // Read reference image
    nifti_image *referenceImage = reg_io_ReadImageFile(inputRefImageName);
    if(referenceImage==NULL){
        reg_print_msg_error("The input reference image could not be read");
        return EXIT_FAILURE;
    }
    reg_tools_changeDatatype<float>(referenceImage);

    // Read floating image
    nifti_image *floatingImage = reg_io_ReadImageFile(inputFloImageName);
    if(floatingImage==NULL){
        reg_print_msg_error("The warped input floating image could not be read");
        return EXIT_FAILURE;
    }
    reg_tools_changeDatatype<float>(floatingImage);

    //create a warped image
    nifti_image *warpedImage = nifti_copy_nim_info(referenceImage);
    warpedImage->data = (void *)malloc(warpedImage->nvox * warpedImage->nbyper);

    //Read affine matrix
    mat44 affineMatrix;
    if(strcmp(affineTransformationName,"-1")!=0) {
        reg_tool_ReadAffineFile(&affineMatrix,
                                affineTransformationName);
    } else {
        // No transformation is specified, an identity transformation is used
        reg_mat44_eye(&affineMatrix);
    }
    // Create a deformation field
    nifti_image *deformationFieldImage = nifti_copy_nim_info(referenceImage);
    deformationFieldImage->dim[0]=deformationFieldImage->ndim=5;
    deformationFieldImage->dim[1]=deformationFieldImage->nx=referenceImage->nx;
    deformationFieldImage->dim[2]=deformationFieldImage->ny=referenceImage->ny;
    deformationFieldImage->dim[3]=deformationFieldImage->nz=referenceImage->nz;
    deformationFieldImage->dim[4]=deformationFieldImage->nt=1;
    deformationFieldImage->pixdim[4]=deformationFieldImage->dt=1.0;
    deformationFieldImage->dim[5]=deformationFieldImage->nu=referenceImage->nz>1?3:2;
    deformationFieldImage->dim[6]=deformationFieldImage->nv=1;
    deformationFieldImage->dim[7]=deformationFieldImage->nw=1;
    deformationFieldImage->nvox =(size_t)deformationFieldImage->nx*
                                 deformationFieldImage->ny*deformationFieldImage->nz*
                                 deformationFieldImage->nt*deformationFieldImage->nu;
    deformationFieldImage->scl_slope=1.f;
    deformationFieldImage->scl_inter=0.f;

    deformationFieldImage->datatype = NIFTI_TYPE_FLOAT32;
    deformationFieldImage->nbyper = sizeof(float);

    deformationFieldImage->data = (void *)calloc(deformationFieldImage->nvox, deformationFieldImage->nbyper);

    // Initialise the deformation field with an identity transformation
    reg_tools_multiplyValueToImage(deformationFieldImage,deformationFieldImage,0.f);
    reg_getDeformationFromDisplacement(deformationFieldImage);
    deformationFieldImage->intent_p1=DEF_FIELD;
    //
    reg_affine_getDeformationField(&affineMatrix,
                                   deformationFieldImage,
                                   false,
                                   NULL);
    // Create an empty mask
    int *maskImage = (int *)calloc(referenceImage->nvox, sizeof(int));
    reg_resampleImage(floatingImage,
                      warpedImage,
                      deformationFieldImage,
                      maskImage,
                      1,
                      0.f);

    free(maskImage);
    nifti_image_free(deformationFieldImage);
    /////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////LETS DO MULTI-RESOLUTION///////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////
    /// WE COULD DO BETTER BECAUSE FOR THE MOMENT WE RESAMPLE TOO MANY TIMES ////////////////
    /////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////
    //int nbLevel = 5;
    //int discrete_radiusArray[] = {30,20,12,6,2};
    //int discrete_incrementArray[] = {5,4,3,2,1};
    //int spacing_voxelArray[] = {8,7,6,5,4};
    int nbLevel = 1;
    int discrete_radiusArray[] = {18};
    int discrete_incrementArray[] = {3};
    int grid_stepArray[] = {8};//

    for(int currentLevel=0;currentLevel<nbLevel;currentLevel++) {
        // Create control point grid image
        int grid_step = grid_stepArray[currentLevel];
        float grid_step_mm[3]={grid_step*referenceImage->dx,
                               grid_step*referenceImage->dy,
                               grid_step*referenceImage->dz};
        nifti_image *controlPointImage = NULL;
        reg_createControlPointGrid<float>(&controlPointImage,
                                          referenceImage,
                                          grid_step_mm);

        memset(controlPointImage->data,0,controlPointImage->nvox*controlPointImage->nbyper);
        reg_tools_multiplyValueToImage(controlPointImage,controlPointImage,0.f);
        reg_getDeformationFromDisplacement(controlPointImage);
        controlPointImage->intent_p1=LIN_SPLINE_GRID;

        // Create an empty mask
        int *mask = (int *)calloc(referenceImage->nvox, sizeof(int));

        // Create a deformation field
        nifti_image *deformationField = nifti_copy_nim_info(referenceImage);
        deformationField->dim[0]=deformationField->ndim=5;
        deformationField->dim[1]=deformationField->nx=referenceImage->nx;
        deformationField->dim[2]=deformationField->ny=referenceImage->ny;
        deformationField->dim[3]=deformationField->nz=referenceImage->nz;
        deformationField->dim[4]=deformationField->nt=1;
        deformationField->pixdim[4]=deformationField->dt=1.0;
        if(referenceImage->nz==1)
            deformationField->dim[5]=deformationField->nu=2;
        else deformationField->dim[5]=deformationField->nu=3;
        deformationField->pixdim[5]=deformationField->du=1.0;
        deformationField->dim[6]=deformationField->nv=1;
        deformationField->pixdim[6]=deformationField->dv=1.0;
        deformationField->dim[7]=deformationField->nw=1;
        deformationField->pixdim[7]=deformationField->dw=1.0;
        deformationField->nvox =
                (size_t)deformationField->nx *
                (size_t)deformationField->ny *
                (size_t)deformationField->nz *
                (size_t)deformationField->nt *
                (size_t)deformationField->nu;
        deformationField->nbyper = sizeof(float);
        deformationField->datatype = NIFTI_TYPE_FLOAT32;
        deformationField->data = (void *)calloc(deformationField->nvox,
                                                deformationField->nbyper);
        deformationField->intent_code=NIFTI_INTENT_VECTOR;
        memset(deformationField->intent_name, 0, 16);
        strcpy(deformationField->intent_name,"NREG_TRANS");
        deformationField->intent_p1=DEF_FIELD;
        deformationField->scl_slope=1.f;
        deformationField->scl_inter=0.f;
        reg_spline_getDeformationField(controlPointImage,
                                       deformationField,
                                       mask,
                                       false, //composition
                                       true // bspline
                                       );
        //
        int mind_length = 12;
        //MINDSSC image
        nifti_image *MINDSSC_refimg = nifti_copy_nim_info(referenceImage);
        MINDSSC_refimg->ndim = MINDSSC_refimg->dim[0] = 4;
        MINDSSC_refimg->nt = MINDSSC_refimg->dim[4] = mind_length;
        MINDSSC_refimg->nvox = MINDSSC_refimg->nvox*mind_length;
        MINDSSC_refimg->data=(void *)calloc(MINDSSC_refimg->nvox,MINDSSC_refimg->nbyper);
        // Compute the MIND descriptor
        GetMINDSSCImageDesciptor(referenceImage,MINDSSC_refimg, mask, 2, 0);
        
        //MINDSSC image
        nifti_image *MINDSSC_warimg = nifti_copy_nim_info(warpedImage);
        MINDSSC_warimg->ndim = MINDSSC_warimg->dim[0] = 4;
        MINDSSC_warimg->nt = MINDSSC_warimg->dim[4] = mind_length;
        MINDSSC_warimg->nvox = MINDSSC_warimg->nvox*mind_length;
        MINDSSC_warimg->data=(void *)calloc(MINDSSC_warimg->nvox,MINDSSC_warimg->nbyper);
        // Compute the MIND descriptor
        GetMINDSSCImageDesciptor(warpedImage,MINDSSC_warimg, mask, 2, 0);
        
        reg_ssd* ssdMeasure = new reg_ssd(true);

        for(int i=0;i<MINDSSC_refimg->nt;++i)
            ssdMeasure->SetActiveTimepoint(i);

        ssdMeasure->InitialiseMeasure(MINDSSC_refimg,
                                      MINDSSC_warimg,
                                      mask,
                                      MINDSSC_warimg,
                                      NULL,
                                      NULL);


        reg_mrf* reg_mrfObject = new reg_mrf(ssdMeasure,
                                             referenceImage,
                                             controlPointImage,
                                             discrete_radiusArray[currentLevel],
                                             discrete_incrementArray[currentLevel],
                                             regularisationWeight);

        reg_mrfObject->Run();
        reg_spline_getDeformationField(controlPointImage,
                                       deformationField,
                                       mask,
                                       false, //composition
                                       true // bspline
                                       );
        reg_resampleImage(floatingImage,
                          warpedImage,
                          deformationField,
                          mask,
                          1,
                          0.f);

        reg_getDisplacementFromDeformation(deformationField);
        deformationField->dim[4] = deformationField->nt = deformationField->nu;
        deformationField->dim[5] = deformationField->nu = 1;
        deformationField->dim[0] = deformationField->ndim = 4;

        warpedImage->cal_min = floatingImage->cal_min;
        warpedImage->cal_max = floatingImage->cal_max;
        //
        reg_io_WriteImageFile(warpedImage, outputImageName);
        //Mr Propre
        delete reg_mrfObject;
        free(mask);
        nifti_image_free(MINDSSC_refimg);
        nifti_image_free(MINDSSC_warimg);
        nifti_image_free(controlPointImage);
        nifti_image_free(deformationField);
    }
    //Mr Propre
    nifti_image_free(referenceImage);
    nifti_image_free(floatingImage);
    nifti_image_free(warpedImage);

    time_t end;
    time(&end);
    int minutes=(int)floorf((end-start)/60.0f);
    int seconds=(int)(end-start - 60*minutes);
    char text[255];
    sprintf(text, "Registration performed in %i min %i sec", minutes, seconds);
    reg_print_info((argv[0]), text);
    reg_print_info((argv[0]), "Have a good day !");

    return EXIT_SUCCESS;
}

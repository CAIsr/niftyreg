/*
 *  reg_f3d.cpp
 *
 *
 *  Created by Marc Modat on 26/03/2009.
 *  Copyright (c) 2009, University College London. All rights reserved.
 *  Centre for Medical Image Computing (CMIC)
 *  See the LICENSE.txt file in the nifty_reg root folder
 *
 */

/* TODO
	- Jacobian log gradient?
*/

/* TOFIX
	- None (so far)
*/

#ifndef _MM_F3D_CPP
#define _MM_F3D_CPP

#include "_reg_resampling.h"
#include "_reg_affineTransformation.h"
#include "_reg_bspline.h"
#include "_reg_mutualinformation.h"
#include "_reg_ssd.h"
#include "_reg_tools.h"

#ifdef _USE_CUDA
	#include "_reg_cudaCommon.h"
	#include "_reg_resampling_gpu.h"
	#include "_reg_affineTransformation_gpu.h"
	#include "_reg_bspline_gpu.h"
	#include "_reg_mutualinformation_gpu.h"
	#include "_reg_tools_gpu.h"
#endif

#ifdef _WINDOWS
    #include <time.h>
#endif

#define PrecisionTYPE float
#define JH_TRI 0
#define JH_PARZEN_WIN 1
#define JH_PW_APPROX 2

typedef struct{
	char *targetImageName;
	char *sourceImageName;
	char *affineMatrixName;
	char *inputCPPName;
    char *targetMaskName;
	float spacing[3];
	int maxIteration;
	int binning;
	int levelNumber;
    int level2Perform;
	float bendingEnergyWeight;
	float jacobianWeight;
	char *outputResultName;
	char *outputCPPName;
	int backgroundIndex[3];
	PrecisionTYPE sourceBGValue;
	float targetSigmaValue;
	float sourceSigmaValue;
}PARAM;
typedef struct{
	bool targetImageFlag;
	bool sourceImageFlag;
	bool affineMatrixFlag;
	bool affineFlirtFlag;
	bool inputCPPFlag;
    bool targetMaskFlag;
	bool spacingFlag[3];
	bool binningFlag;
	bool levelNumberFlag;
    bool level2PerformFlag;
	bool maxIterationFlag;
	bool outputResultFlag;
	bool outputCPPFlag;
	bool backgroundIndexFlag;
	
	bool bendingEnergyFlag;
	bool appBendingEnergyFlag;
	bool beGradFlag;

	bool jacobianWeightFlag;
	bool appJacobianFlag;
	bool jacFullFlag;
	bool jacGradFlag;

	bool useSSDFlag;
	bool noConjugateGradient;
	bool targetSigmaFlag;
	bool sourceSigmaFlag;
	bool pyramidFlag;

#ifdef _USE_CUDA	
	bool useGPUFlag;
	bool memoryFlag;
#endif
}FLAG;


void PetitUsage(char *exec)
{
	fprintf(stderr,"Fast Free-Form Deformation algorithm for non-rigid registration.\n");
	fprintf(stderr,"Usage:\t%s -target <targetImageName> -source <sourceImageName> [OPTIONS].\n",exec);
	fprintf(stderr,"\tSee the help for more details (-h).\n");
	return;
}
void Usage(char *exec)
{
	printf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
	printf("Fast Free-Form Deformation algorithm for non-rigid registration.\n");
    printf("This implementation is a re-factoring off Daniel Rueckert' 99 TMI work.\n");
    printf("The code is presented in Modat et al., \"Fast Free-Form Deformation using\n");
    printf("graphics processing units\", CMPB, 2009\n");
	printf("Cubic B-Spline are used to deform a source image in order to optimise a objective function\n");
	printf("based on the Normalised Mutual Information and a penalty term. The penalty term could\n");
	printf("be either the bending energy or the absolute Jacobian determinant log.\n");
	printf("This code has been written by Marc Modat (m.modat@ucl.ac.uk), for any comment,\n");
	printf("please contact him.\n");
	printf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
	printf("Usage:\t%s -target <filename> -source <filename> [OPTIONS].\n",exec);
	printf("\t-target <filename>\tFilename of the target image (mandatory)\n");
	printf("\t-source <filename>\tFilename of the source image (mandatory)\n");
	printf("* * OPTIONS * *\n");
	printf("\t-incpp <filename>\tFilename of control point grid input\n\t\t\t\tThe coarse spacing is defined by this file.\n");
	printf("\t-result <filename> \tFilename of the resampled image [outputResult.nii]\n");
	printf("\t-cpp <filename>\t\tFilename of control point grid [outputCPP.nii]\n");
	printf("\t-aff <filename>\t\tFilename which contains an affine transformation (Affine*Target=Source)\n");
    printf("\t-affFlirt <filename>\tFilename which contains a flirt affine transformation\n");
    printf("\t-tmask <filename>\tFilename of a mask image in the target space\n");
	printf("\t-maxit <int>\t\tMaximal number of iteration per level [300]\n");
	printf("\t-sx <float>\t\tFinal grid spacing along the x axis in mm [5]\n");
	printf("\t-sy <float>\t\tFinal grid spacing along the y axis in mm [sx value]\n");
	printf("\t-sz <float>\t\tFinal grid spacing along the z axis in mm [sx value]\n");
	printf("\t-bin <int>\t\tNumber of bin to use for the joint histogram [64]\n");
	printf("\t-smooT <float>\t\tSmooth the target image using the specified sigma (mm) [0]\n");
	printf("\t-smooS <float>\t\tSmooth the source image using the specified sigma (mm) [0]\n");
	printf("\t-ln <int>\t\tNumber of level to perform [3]\n");
    printf("\t-lp <int>\t\tOnly perform the first levels [ln]\n");
	printf("\t-nopy\t\t\tDo not use a pyramidal approach [no]\n");
	
	printf("\t-be <float>\t\tWeight of the bending energy penalty term [0.01]\n");
	printf("\t-noAppBE\t\t\tTo not approximate the BE value only at the control point position\n");
	printf("\t-noGradBE\t\t\tTo not use the gradient of the bending energy\n");
	
// 	printf("\t-jl <float>\t\tWeight of log of the Jacobian determinant penalty term [0.0]\n");
// 	printf("\t-appJL\t\t\tApproximate the JL value only at the control point position [no]\n");
// 	printf("\t-fullJL\t\t\tThe JL is compute using the full resolution image [no]\n");
// 	printf("\t-gradJL\t\t\tTo use the gradient of the Jacobian determinant [no]\n");
	
	printf("\t-bgi <int> <int> <int>\tForce the background value during\n\t\t\t\tresampling to have the same value as this voxel in the source image [none]\n");
// 	printf("\t-ssd\t\t\tTo use the SSD as the similiarity measure [no]\n");
	printf("\t-noConj\t\t\tTo not use the conjuage gradient optimisation but a simple gradient ascent/descent\n");
	printf("\t-mem\t\t\tDisplay an approximate memory requierment and exit\n");

#ifdef _USE_CUDA
	printf("\t-gpu \t\t\tTo use the GPU implementation [no]\n");
#endif
	printf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
	return;
}

int main(int argc, char **argv)
{
	time_t start; time(&start);
	
	PARAM *param = (PARAM *)calloc(1,sizeof(PARAM));
	FLAG *flag = (FLAG *)calloc(1,sizeof(FLAG));

	flag->pyramidFlag=1;
	flag->bendingEnergyFlag=1;
	param->bendingEnergyWeight=0.01f;
	flag->appBendingEnergyFlag=1;
	flag->beGradFlag=1;

	/* read the input parameter */
	for(int i=1;i<argc;i++){
		if(strcmp(argv[i], "-help")==0 || strcmp(argv[i], "-Help")==0 ||
			strcmp(argv[i], "-HELP")==0 || strcmp(argv[i], "-h")==0 ||
			strcmp(argv[i], "--h")==0 || strcmp(argv[i], "--help")==0){
			Usage(argv[0]);
			return 0;
		}
		else if(strcmp(argv[i], "-target") == 0){
			param->targetImageName=argv[++i];
			flag->targetImageFlag=1;
		}
		else if(strcmp(argv[i], "-source") == 0){
			param->sourceImageName=argv[++i];
			flag->sourceImageFlag=1;
		}
		else if(strcmp(argv[i], "-aff") == 0){
			param->affineMatrixName=argv[++i];
			flag->affineMatrixFlag=1;
		}
		else if(strcmp(argv[i], "-affFlirt") == 0){
			param->affineMatrixName=argv[++i];
			flag->affineMatrixFlag=1;
			flag->affineFlirtFlag=1;
		}
        else if(strcmp(argv[i], "-incpp") == 0){
            param->inputCPPName=argv[++i];
            flag->inputCPPFlag=1;
        }
        else if(strcmp(argv[i], "-tmask") == 0){
            param->targetMaskName=argv[++i];
            flag->targetMaskFlag=1;
        }
		else if(strcmp(argv[i], "-result") == 0){
			param->outputResultName=argv[++i];
			flag->outputResultFlag=1;
		}
		else if(strcmp(argv[i], "-cpp") == 0){
			param->outputCPPName=argv[++i];
			flag->outputCPPFlag=1;
		}
		else if(strcmp(argv[i], "-maxit") == 0){
			param->maxIteration=atoi(argv[++i]);
			flag->maxIterationFlag=1;
		}
		else if(strcmp(argv[i], "-sx") == 0){
			param->spacing[0]=(float)(atof(argv[++i]));
			flag->spacingFlag[0]=1;
		}
		else if(strcmp(argv[i], "-sy") == 0){
			param->spacing[1]=(float)(atof(argv[++i]));
			flag->spacingFlag[1]=1;
		}
		else if(strcmp(argv[i], "-sz") == 0){
			param->spacing[2]=(float)(atof(argv[++i]));
			flag->spacingFlag[2]=1;
		}
		else if(strcmp(argv[i], "-bin") == 0){
			param->binning=atoi(argv[++i]);
			flag->binningFlag=1;
		}
		else if(strcmp(argv[i], "-ln") == 0){
			param->levelNumber=atoi(argv[++i]);
			flag->levelNumberFlag=1;
		}
        else if(strcmp(argv[i], "-lp") == 0){
            param->level2Perform=atoi(argv[++i]);
            flag->level2PerformFlag=1;
        }
		else if(strcmp(argv[i], "-nopy") == 0){
			flag->pyramidFlag=0;
		}
		else if(strcmp(argv[i], "-be") == 0){
			param->bendingEnergyWeight=(float)(atof(argv[++i]));
		}
		else if(strcmp(argv[i], "-noAppBE") == 0){
			flag->appBendingEnergyFlag=0;
		}
		else if(strcmp(argv[i], "-noGradBE") == 0){
			flag->beGradFlag=0;
		}
		else if(strcmp(argv[i], "-jl") == 0){
			param->jacobianWeight=(float)(atof(argv[++i]));
			flag->jacobianWeightFlag=1;
		}
		else if(strcmp(argv[i], "-appJL") == 0){
			flag->appJacobianFlag=1;
		}
		else if(strcmp(argv[i], "-fullJL") == 0){
			flag->jacFullFlag=1;
		}
		else if(strcmp(argv[i], "-gradJL") == 0){
			flag->jacGradFlag=1;
		}
		else if(strcmp(argv[i], "-noConj") == 0){
			flag->noConjugateGradient=1;
		}
		else if(strcmp(argv[i], "-smooT") == 0){
			param->targetSigmaValue=(float)(atof(argv[++i]));
			flag->targetSigmaFlag=1;
		}
		else if(strcmp(argv[i], "-smooS") == 0){
			param->sourceSigmaValue=(float)(atof(argv[++i]));
			flag->sourceSigmaFlag=1;
		}
		else if(strcmp(argv[i], "-bgi") == 0){
			param->backgroundIndex[0]=atoi(argv[++i]);
			param->backgroundIndex[1]=atoi(argv[++i]);
			param->backgroundIndex[2]=atoi(argv[++i]);
			flag->backgroundIndexFlag=1;
		}
		else if(strcmp(argv[i], "-ssd") == 0){
			flag->useSSDFlag=1;
			printf("I'm affraid I did not have time to validate the SSD metric and its gradient yet\n");
			return 1;
		}
#ifdef _USE_CUDA
		else if(strcmp(argv[i], "-mem") == 0){
			flag->memoryFlag=1;
		}
		else if(strcmp(argv[i], "-gpu") == 0){
			flag->useGPUFlag=1;
		}
#endif
		else{
			fprintf(stderr,"Err:\tParameter %s unknown.\n",argv[i]);
			PetitUsage(argv[0]);
			return 1;
		}
	}

#ifdef _USE_CUDA
	if(flag->useGPUFlag){
		if(flag->jacobianWeightFlag){
			printf("\n[WARNING] The gpu-based Jacobian determinant log penalty term has not been implemented yet [/WARNING]\n");
			printf("[WARNING] ... Exit ... [/WARNING]\n");
			return 1;
		}
		if(!flag->bendingEnergyFlag){
			printf("\n[WARNING] The gpu-based bending-energy is only approximated [/WARNING]\n");
			flag->appBendingEnergyFlag=1;
		}
	}
#endif

	if(!flag->targetImageFlag || !flag->sourceImageFlag){
		fprintf(stderr,"Err:\tThe target and the source image have to be defined.\n");
		PetitUsage(argv[0]);
		return 1;
	}

	/* Read the grid spacing */
	if(!flag->spacingFlag[0]) param->spacing[0]=5.0;
	if(!flag->spacingFlag[1]) param->spacing[1]=param->spacing[0];
	if(!flag->spacingFlag[2]) param->spacing[2]=param->spacing[0];
	
	if(!flag->levelNumberFlag) param->levelNumber=3;
    if(!flag->level2PerformFlag) param->level2Perform=param->levelNumber;
    param->level2Perform=param->level2Perform<param->levelNumber?param->level2Perform:param->levelNumber;

	/* Read the maximum number of iteration */
	if(!flag->maxIterationFlag) param->maxIteration=300;

	/* Read the number of bin to use */
	if(!flag->binningFlag) param->binning=64;
    param->binning += 4; //This is due to the extrapolation of the joint histogram using the Parzen window

    if(param->bendingEnergyWeight==0.0f)flag->bendingEnergyFlag=0;

	nifti_image *targetHeader = nifti_image_read(param->targetImageName,false);
	if(targetHeader == NULL){
		fprintf(stderr,"* ERROR Error when reading the target image: %s\n",param->targetImageName);
		return 1;
	}
	nifti_image *sourceHeader = nifti_image_read(param->sourceImageName,false);
	if(sourceHeader == NULL){
		fprintf(stderr,"* ERROR Error when reading the source image: %s\n",param->sourceImageName);
		return 1;
	}

	/* Check the source background index */
	if(!flag->backgroundIndexFlag) param->sourceBGValue = 0.0;
	else{
		if(param->backgroundIndex[0] < 0 || param->backgroundIndex[1] < 0 || param->backgroundIndex[2] < 0 
		   || param->backgroundIndex[0] >= sourceHeader->dim[1] || param->backgroundIndex[1] >= sourceHeader->dim[2] || param->backgroundIndex[2] >= sourceHeader->dim[3]){
			fprintf(stderr,"The specified index (%i %i %i) for background does not belong to the source image (out of bondary)\n",
					param->backgroundIndex[0], param->backgroundIndex[1], param->backgroundIndex[2]);
			return 1;
		}
	}

#ifdef _USE_CUDA
    // Compute the ratio if the registration is not performed using
    // the full resolution image
    float ratioFullRes = 1.0f;
    if(param->level2Perform != param->levelNumber){
        ratioFullRes= 1.0f/powf(8.0f,(float)(param->levelNumber-param->level2Perform));
    }
	float memoryNeeded=0;
    memoryNeeded += 2 * targetHeader->nvox * sizeof(float) * ratioFullRes; // target and result images
    memoryNeeded += targetHeader->nvox * sizeof(bool) * ratioFullRes; // target mask
	memoryNeeded += sourceHeader->nvox * sizeof(float) * ratioFullRes; // source image
	memoryNeeded += targetHeader->nvox * sizeof(float4) * ratioFullRes; // position field
	memoryNeeded += targetHeader->nvox * sizeof(float4) * ratioFullRes; // spatial gradient
	memoryNeeded += 2 * targetHeader->nvox * sizeof(float4) * ratioFullRes; // nmi gradient + smoothed
	memoryNeeded += 4 * (ceil(targetHeader->nx*targetHeader->dx/param->spacing[0])+4) *
			(ceil(targetHeader->nx*targetHeader->dy/param->spacing[1])+4) *
			(ceil(targetHeader->nx*targetHeader->dz/param->spacing[2])+4) *
			sizeof(float4);// control point image + cpp gradient image + 2 conjugate array
	memoryNeeded += param->binning*(param->binning+2)*sizeof(double); // joint histo
		
	if(flag->memoryFlag){
		printf("The approximate amount of gpu memory require to run this registration is %g Mo\n", memoryNeeded / 1000000.0);
		return 0;
	}
#endif

	/* Read the affine tranformation is defined otherwise assign it to identity */
	mat44 *affineTransformation;
	if(!flag->inputCPPFlag){
		affineTransformation = (mat44 *)calloc(1,sizeof(mat44));
		affineTransformation->m[0][0]=1.0;
		affineTransformation->m[1][1]=1.0;
		affineTransformation->m[2][2]=1.0;
		affineTransformation->m[3][3]=1.0;
		if(flag->affineMatrixFlag){
			// Check first if the specified affine file exist
			if(FILE *aff=fopen(param->affineMatrixName, "r")){
				fclose(aff);
			}
			else{
				fprintf(stderr,"The specified input affine file (%s) can not be read\n",param->affineMatrixName);
				return 1;
			}
			reg_tool_ReadAffineFile(	affineTransformation,
						targetHeader,
						sourceHeader,
						param->affineMatrixName,
						flag->affineFlirtFlag);
		}
#ifdef _VERBOSE
        reg_mat44_disp(affineTransformation, "[VERBOSE] Affine transformation matrix");
#endif
	}

	/* read the control point image */
	nifti_image *controlPointImage;
	if(flag->inputCPPFlag){
		controlPointImage = nifti_image_read(param->inputCPPName,true);
		if(controlPointImage == NULL){
			fprintf(stderr,"* ERROR Error when reading the input cpp image: %s\n",param->inputCPPName);
			return 1;
		}
	}
    if(!flag->outputCPPFlag) param->outputCPPName="outputCPP.nii";

    /* read and binarise the target mask image */
    nifti_image *targetMaskImage;
    if(flag->targetMaskFlag){
        targetMaskImage = nifti_image_read(param->targetMaskName,true);
        if(targetMaskImage == NULL){
            fprintf(stderr,"* ERROR Error when reading the target naask image: %s\n",param->targetMaskName);
            return 1;
        }
        /* check the dimension */
        for(int i=1; i<=targetHeader->dim[0]; i++){
            if(targetHeader->dim[i]!=targetMaskImage->dim[i]){
                fprintf(stderr,"* ERROR The target image and its mask do not have the same dimension\n");
                return 1;
            }
        }
        reg_tool_binarise_image(targetMaskImage);
    }


	/* ****************** */
	/* DISPLAY THE REGISTRATION PARAMETERS */
	/* ****************** */
	printf("\n* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
	printf("Command line:\n %s",argv[0]);
	for(int i=1;i<argc;i++)
		printf(" %s",argv[i]);
	printf("\n\n");
	printf("Parameters\n");
	printf("Target image name: %s\n",targetHeader->fname);
	printf("\t%ix%ix%i voxels\n",targetHeader->nx,targetHeader->ny,targetHeader->nz);
	printf("\t%gx%gx%g mm\n",targetHeader->dx,targetHeader->dy,targetHeader->dz);
	printf("Source image name: %s\n",sourceHeader->fname);
	printf("\t%ix%ix%i voxels\n",sourceHeader->nx,sourceHeader->ny,sourceHeader->nz);
	printf("\t%gx%gx%g mm\n",sourceHeader->dx,sourceHeader->dy,sourceHeader->dz);
	printf("Maximum iteration number: %i\n",param->maxIteration);
	printf("Number of bin to used: %i\n",param->binning);
	printf("Bending energy weight: %g\n",param->bendingEnergyWeight);
	if(flag->appBendingEnergyFlag) printf("Bending energy penalty term evaluated at the control point position only\n");
	printf("log of the jacobian determinant weight: %g\n",param->jacobianWeight);
	if(flag->appJacobianFlag) printf("Log of the Jacobian penalty term evaluated at the control point position only\n");
	if(flag->beGradFlag) printf("The gradient of the bending energy is used\n");
	if(flag->jacGradFlag) printf("The gradient of the jacobian determinant is used\n");
#ifdef _USE_CUDA
	if(flag->useGPUFlag) printf("The GPU implementation is used\n");
	else printf("The CPU implementation is used\n");
#endif
	printf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n\n");
	
	/* ********************** */
	/* START THE REGISTRATION */
	/* ********************** */

#ifdef _USE_CUDA
	if(flag->useGPUFlag){
		struct cudaDeviceProp deviceProp;
		// following code is from cutGetMaxGflopsDeviceId()
		int device_count = 0;
		cudaGetDeviceCount( &device_count );
	
		int max_gflops_device = 0;
		int max_gflops = 0;
		
		int current_device = 0;
		cudaGetDeviceProperties( &deviceProp, current_device );
		max_gflops = deviceProp.multiProcessorCount * deviceProp.clockRate;
		++current_device;
	
		while( current_device < device_count )
		{
			cudaGetDeviceProperties( &deviceProp, current_device );
			int gflops = deviceProp.multiProcessorCount * deviceProp.clockRate;
			if( gflops > max_gflops )
			{
				max_gflops        = gflops;
				max_gflops_device = current_device;
			}
			++current_device;
		}
		const int device = max_gflops_device;

		CUDA_SAFE_CALL(cudaSetDevice( device ));
		CUDA_SAFE_CALL(cudaGetDeviceProperties(&deviceProp, device ));
		if (deviceProp.major < 1){
			printf("ERROR\tThe specified graphical card does not exist.\n");
			return 1;
		}
#ifdef _VERBOSE
		printf("[VERBOSE] Graphical card memory[%i/%i] = %iMo avail | %iMo required.\n", device+1, device_count,
			(int)floor(deviceProp.totalGlobalMem/1000000.0), (int)ceil(memoryNeeded/1000000.0));
#endif
	}
#endif

    for(int level=0; level<param->level2Perform; level++){
        /* Read the target and source image */
        nifti_image *targetImage = nifti_image_read(param->targetImageName,true);
        if(targetImage->data == NULL){
            fprintf(stderr, "* ERROR Error when reading the target image: %s\n", param->targetImageName);
            return 1;
        }
        reg_changeDatatype<PrecisionTYPE>(targetImage);
        nifti_image *sourceImage = nifti_image_read(param->sourceImageName,true);
        if(sourceImage->data == NULL){
            fprintf(stderr, "* ERROR Error when reading the source image: %s\n", param->sourceImageName);
            return 1;
        }
        reg_changeDatatype<PrecisionTYPE>(sourceImage);

        /* declare the target mask array */
        int *targetMask;
        int activeVoxelNumber=0;

        /* downsample the input images if appropriate */
        if(flag->pyramidFlag){
            nifti_image *tempMaskImage;
            if(flag->targetMaskFlag){
                tempMaskImage = nifti_copy_nim_info(targetMaskImage);
                tempMaskImage->data = (void *)malloc(tempMaskImage->nvox * tempMaskImage->nbyper);
                memcpy(tempMaskImage->data, targetMaskImage->data, tempMaskImage->nvox*tempMaskImage->nbyper);
            }
            for(int l=level; l<param->levelNumber-1; l++){
                reg_downsampleImage<PrecisionTYPE>(targetImage, 1);
                reg_downsampleImage<PrecisionTYPE>(sourceImage, 1);
                if(flag->targetMaskFlag){
                    reg_downsampleImage<PrecisionTYPE>(tempMaskImage, 0);
                }
            }
            targetMask = (int *)malloc(targetImage->nvox*sizeof(int));
            if(flag->targetMaskFlag){
                reg_tool_binaryImage2int(tempMaskImage, targetMask, activeVoxelNumber);
                nifti_image_free(tempMaskImage);
            }
            else{
                for(int i=0; i<targetImage->nvox; i++)
                    targetMask[i]=i;
                activeVoxelNumber=targetImage->nvox;
            }
        }


        /* smooth the input image if appropriate */
        if(flag->targetSigmaFlag)
            reg_gaussianSmoothing<PrecisionTYPE>( targetImage, param->targetSigmaValue);
        if(flag->sourceSigmaFlag)
            reg_gaussianSmoothing<PrecisionTYPE>( sourceImage, param->sourceSigmaValue);

        if(level==0){
            if(!flag->inputCPPFlag){
                /* allocate the control point image */
                float gridSpacing[3];
                gridSpacing[0] = param->spacing[0] * powf(2.0f, (float)(param->level2Perform-1));
                gridSpacing[1] = param->spacing[1] * powf(2.0f, (float)(param->level2Perform-1));
                gridSpacing[2] = param->spacing[2] * powf(2.0f, (float)(param->level2Perform-1));

                int dim_cpp[8];
                dim_cpp[0]=5;
                dim_cpp[1]=(int)floor(targetImage->nx*targetImage->dx/gridSpacing[0])+4;
                dim_cpp[2]=(int)floor(targetImage->ny*targetImage->dy/gridSpacing[1])+4;
                dim_cpp[3]=(int)floor(targetImage->nz*targetImage->dz/gridSpacing[2])+4;
                dim_cpp[5]=3;
                dim_cpp[4]=dim_cpp[6]=dim_cpp[7]=1;
                if(sizeof(PrecisionTYPE)==4) controlPointImage = nifti_make_new_nim(dim_cpp, NIFTI_TYPE_FLOAT32, true);
                else controlPointImage = nifti_make_new_nim(dim_cpp, NIFTI_TYPE_FLOAT64, true);
                controlPointImage->cal_min=0;
                controlPointImage->cal_max=0;
                controlPointImage->pixdim[0]=1.0f;
                controlPointImage->pixdim[1]=controlPointImage->dx=gridSpacing[0];
                controlPointImage->pixdim[2]=controlPointImage->dy=gridSpacing[1];
                controlPointImage->pixdim[3]=controlPointImage->dz=gridSpacing[2];
                controlPointImage->pixdim[4]=controlPointImage->dt=1.0f;
                controlPointImage->pixdim[5]=controlPointImage->du=1.0f;
                controlPointImage->pixdim[6]=controlPointImage->dv=1.0f;
                controlPointImage->pixdim[7]=controlPointImage->dw=1.0f;
                controlPointImage->qform_code=targetImage->qform_code;
                controlPointImage->sform_code=targetImage->sform_code;
            }
        }
        else{
            // the grid as to be refined
            reg_bspline_refineControlPointGrid(targetImage, controlPointImage);
        }

        float qb, qc, qd, qx, qy, qz, dx, dy, dz, qfac;
        nifti_mat44_to_quatern( targetImage->qto_xyz, &qb, &qc, &qd, &qx, &qy, &qz, &dx, &dy, &dz, &qfac);
        controlPointImage->quatern_b=qb;
        controlPointImage->quatern_c=qc;
        controlPointImage->quatern_d=qd;
        controlPointImage->qfac=qfac;

        controlPointImage->qto_xyz = nifti_quatern_to_mat44(qb, qc, qd, qx, qy, qz,
            controlPointImage->dx, controlPointImage->dy, controlPointImage->dz, qfac);

        // Origin is shifted from 1 control point
        float originIndex[3];
        float originReal[3];
        originIndex[0] = -1.0f;
        originIndex[1] = -1.0f;
        originIndex[2] = -1.0f;
        reg_mat44_mul(&(controlPointImage->qto_xyz), originIndex, originReal);
        controlPointImage->qto_xyz.m[0][3] = controlPointImage->qoffset_x = originReal[0];
        controlPointImage->qto_xyz.m[1][3] = controlPointImage->qoffset_y = originReal[1];
        controlPointImage->qto_xyz.m[2][3] = controlPointImage->qoffset_z = originReal[2];

        controlPointImage->qto_ijk = nifti_mat44_inverse(controlPointImage->qto_xyz);

        if(controlPointImage->sform_code>0){
            float target2gridRatio[3];
            target2gridRatio[0] = controlPointImage->dx / targetImage->dx;
            target2gridRatio[1] = controlPointImage->dy / targetImage->dy;
            target2gridRatio[2] = controlPointImage->dz / targetImage->dz;
            for(int i=0; i<3; i++){
                controlPointImage->sto_xyz.m[i][0] = targetImage->sto_xyz.m[i][0] * target2gridRatio[0];
                controlPointImage->sto_xyz.m[i][1] = targetImage->sto_xyz.m[i][1] * target2gridRatio[1];
                controlPointImage->sto_xyz.m[i][2] = targetImage->sto_xyz.m[i][2] * target2gridRatio[2];
            }
            controlPointImage->sto_xyz.m[0][3] = targetImage->sto_xyz.m[0][3];
            controlPointImage->sto_xyz.m[1][3] = targetImage->sto_xyz.m[1][3];
            controlPointImage->sto_xyz.m[2][3] = targetImage->sto_xyz.m[2][3];
            controlPointImage->sto_xyz.m[3][3] = 1.0f;
            // Origin is shifted from 1 control point
            reg_mat44_mul(&(controlPointImage->sto_xyz), originIndex, originReal);
            controlPointImage->sto_xyz.m[0][3]=originReal[0];
            controlPointImage->sto_xyz.m[1][3]=originReal[1];
            controlPointImage->sto_xyz.m[2][3]=originReal[2];

            controlPointImage->sto_ijk = nifti_mat44_inverse(controlPointImage->sto_xyz);
        }

        if(level==0 && !flag->inputCPPFlag){
            if(reg_bspline_initialiseControlPointGridWithAffine(affineTransformation, controlPointImage)) return 1;
            free(affineTransformation);
        }

        mat44 *cppMatrix_xyz;
        mat44 *targetMatrix_ijk;
        mat44 *sourceMatrix_xyz;
        if(controlPointImage->sform_code)
            cppMatrix_xyz = &(controlPointImage->sto_xyz);
        else cppMatrix_xyz = &(controlPointImage->qto_xyz);
        if(targetImage->sform_code)
            targetMatrix_ijk = &(targetImage->sto_ijk);
        else targetMatrix_ijk = &(targetImage->qto_ijk);
        if(sourceImage->sform_code)
            sourceMatrix_xyz = &(sourceImage->sto_xyz);
        else sourceMatrix_xyz = &(sourceImage->qto_xyz);

        /* allocate the deformation Field image */
        nifti_image *positionFieldImage = nifti_copy_nim_info(targetImage);
        positionFieldImage->dim[0]=positionFieldImage->ndim=5;
        positionFieldImage->dim[1]=positionFieldImage->nx=targetImage->nx;
        positionFieldImage->dim[2]=positionFieldImage->ny=targetImage->ny;
        positionFieldImage->dim[3]=positionFieldImage->nz=targetImage->nz;
        positionFieldImage->dim[4]=positionFieldImage->nt=1;positionFieldImage->pixdim[4]=positionFieldImage->dt=1.0;
        positionFieldImage->dim[5]=positionFieldImage->nu=3;positionFieldImage->pixdim[5]=positionFieldImage->du=1.0;
        positionFieldImage->dim[6]=positionFieldImage->nv=1;positionFieldImage->pixdim[6]=positionFieldImage->dv=1.0;
        positionFieldImage->dim[7]=positionFieldImage->nw=1;positionFieldImage->pixdim[7]=positionFieldImage->dw=1.0;
        positionFieldImage->nvox=positionFieldImage->nx*positionFieldImage->ny*positionFieldImage->nz*positionFieldImage->nt*positionFieldImage->nu;
        if(sizeof(PrecisionTYPE)==4) positionFieldImage->datatype = NIFTI_TYPE_FLOAT32;
        else positionFieldImage->datatype = NIFTI_TYPE_FLOAT64;
        positionFieldImage->nbyper = sizeof(PrecisionTYPE);
        positionFieldImage->data = (void *)calloc(positionFieldImage->nvox, positionFieldImage->nbyper);

        /* allocate the result image */
        nifti_image *resultImage = nifti_copy_nim_info(targetImage);
        resultImage->datatype = sourceImage->datatype;
        resultImage->nbyper = sourceImage->nbyper;
#ifdef _USE_CUDA
        if(flag->useGPUFlag){
            CUDA_SAFE_CALL(cudaMallocHost((void **)&(resultImage->data), resultImage->nvox*resultImage->nbyper));
        }
		else
#endif
			resultImage->data = (void *)calloc(resultImage->nvox, resultImage->nbyper);

		printf("Current level %i / %i\n", level+1, param->levelNumber);
		printf("Target image size: \t%ix%ix%i voxels\t%gx%gx%g mm\n",
		       targetImage->nx, targetImage->ny, targetImage->nz, targetImage->dx, targetImage->dy, targetImage->dz);
		printf("Source image size: \t%ix%ix%i voxels\t%gx%gx%g mm\n",
		       sourceImage->nx, sourceImage->ny, sourceImage->nz, sourceImage->dx, sourceImage->dy, sourceImage->dz);
		printf("Control point position image name: %s\n",param->outputCPPName);
		printf("\t%ix%ix%i control points (%i DoF)\n",controlPointImage->nx,controlPointImage->ny,controlPointImage->nz,(int)controlPointImage->nvox);
		printf("\t%gx%gx%g mm\n",controlPointImage->dx,controlPointImage->dy,controlPointImage->dz);	
#ifdef _VERBOSE
		if(targetImage->sform_code>0)
			reg_mat44_disp(&targetImage->sto_xyz, "[VERBOSE] Target image matrix");
		else reg_mat44_disp(&targetImage->qto_xyz, "[VERBOSE] Target image matrix");
		reg_mat44_disp(sourceMatrix_xyz, "[VERBOSE] Source image matrix");
		reg_mat44_disp(cppMatrix_xyz, "[VERBOSE] Control point image matrix");
#endif
		printf("* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
		
		float maxStepSize = (targetImage->dx>targetImage->dy)?targetImage->dx:targetImage->dy;
		maxStepSize = (targetImage->dz>maxStepSize)?targetImage->dz:maxStepSize;
		float currentSize = maxStepSize;
		float smallestSize = maxStepSize / 100.0f;

		/* the target and source are resampled between 0 and bin-1
         * The images are then shifted by two which is the suport of the spline used
         * by the parzen window filling of the joint histogram */
		reg_intensityRescale(targetImage,2.0f,(float)param->binning+1.0f);
		reg_intensityRescale(sourceImage,2.0f,(float)param->binning+1.0f);

        if(flag->backgroundIndexFlag){
            int index[3];
            index[0]=param->backgroundIndex[0];
            index[1]=param->backgroundIndex[1];
            index[2]=param->backgroundIndex[2];
            if(flag->pyramidFlag){
                for(int l=level; l<param->levelNumber-1; l++){
                    index[0] /= 2;
                    index[1] /= 2;
                    index[2] /= 2;
                }
            }
            param->sourceBGValue = (float)(reg_tool_GetIntensityValue(sourceImage, index));
        }

        /* the gradient images are allocated */
        nifti_image *resultGradientImage;
        nifti_image *voxelNMIGradientImage;
        nifti_image *nodeNMIGradientImage;
        /* Conjugate gradient */
        PrecisionTYPE *conjugateG;
        PrecisionTYPE *conjugateH;
        /* joint histogram related variables */
        double *probaJointHistogram = (double *)malloc(param->binning*(param->binning+2)*sizeof(double));
        double *logJointHistogram = (double *)malloc(param->binning*(param->binning+2)*sizeof(double));
        double *entropies = (double *)malloc(4*sizeof(double));

        PrecisionTYPE *bestControlPointPosition;

#ifdef _USE_CUDA
        float *targetImageArray_d;
        cudaArray *sourceImageArray_d;
        float4 *controlPointImageArray_d;
        float *resultImageArray_d;
        float4 *positionFieldImageArray_d;
        int *targetMask_d;

        float4 *resultGradientArray_d;
        float4 *voxelNMIGradientArray_d;
        float4 *nodeNMIGradientArray_d;

        float4 *conjugateG_d;
        float4 *conjugateH_d;

        float4 *bestControlPointPosition_d;

        float *logJointHistogram_d;

		if(flag->useGPUFlag){
			if(!flag->noConjugateGradient){
				if(cudaCommon_allocateArrayToDevice(&conjugateG_d, controlPointImage->dim)) return 1;
				if(cudaCommon_allocateArrayToDevice(&conjugateH_d, controlPointImage->dim)) return 1;
			}
			if(cudaCommon_allocateArrayToDevice<float>(&targetImageArray_d, targetImage->dim)) return 1;
			if(cudaCommon_transferNiftiToArrayOnDevice<float>(&targetImageArray_d, targetImage)) return 1;

			if(cudaCommon_allocateArrayToDevice<float>(&sourceImageArray_d, sourceImage->dim)) return 1;
			if(cudaCommon_transferNiftiToArrayOnDevice<float>(&sourceImageArray_d,sourceImage)) return 1;

            if(cudaCommon_allocateArrayToDevice<float>(&resultImageArray_d, targetImage->dim)) return 1;

			if(cudaCommon_allocateArrayToDevice<float4>(&controlPointImageArray_d, controlPointImage->dim)) return 1;
			if(cudaCommon_transferNiftiToArrayOnDevice<float4>(&controlPointImageArray_d,controlPointImage)) return 1;

			if(cudaCommon_allocateArrayToDevice<float4>(&bestControlPointPosition_d, controlPointImage->dim)) return 1;
			if(cudaCommon_transferNiftiToArrayOnDevice<float4>(&bestControlPointPosition_d,controlPointImage)) return 1;

			CUDA_SAFE_CALL(cudaMalloc((void **)&logJointHistogram_d, param->binning*(param->binning+2)*sizeof(float)));

            if(cudaCommon_allocateArrayToDevice(&voxelNMIGradientArray_d, resultImage->dim)) return 1;
            if(cudaCommon_allocateArrayToDevice(&nodeNMIGradientArray_d, controlPointImage->dim)) return 1;

            // Index of the active voxel is stored
            int *targetMask_h;CUDA_SAFE_CALL(cudaMallocHost((void **)&targetMask_h, activeVoxelNumber*sizeof(int)));
            int *targetMask_h_ptr = &targetMask_h[0];
            for(int i=0;i<targetImage->nvox;i++){
                if(targetMask[i]!=-1) *targetMask_h_ptr++=i;
            }
            CUDA_SAFE_CALL(cudaMalloc((void **)&targetMask_d, activeVoxelNumber*sizeof(int)));
            CUDA_SAFE_CALL(cudaMemcpy(targetMask_d, targetMask_h, activeVoxelNumber*sizeof(int), cudaMemcpyHostToDevice));
            CUDA_SAFE_CALL(cudaFreeHost(targetMask_h));

            CUDA_SAFE_CALL(cudaMalloc((void **)&positionFieldImageArray_d, activeVoxelNumber*sizeof(float4)));
            CUDA_SAFE_CALL(cudaMalloc((void **)&resultGradientArray_d, activeVoxelNumber*sizeof(float4)));

		}
		else{
#endif
			resultGradientImage = nifti_copy_nim_info(positionFieldImage);
			if(sizeof(PrecisionTYPE)==4) resultGradientImage->datatype = NIFTI_TYPE_FLOAT32;
			else resultGradientImage->datatype = NIFTI_TYPE_FLOAT64;
			resultGradientImage->nbyper = sizeof(PrecisionTYPE);
			resultGradientImage->data = (void *)calloc(resultGradientImage->nvox, resultGradientImage->nbyper);
			voxelNMIGradientImage = nifti_copy_nim_info(positionFieldImage);
			if(sizeof(PrecisionTYPE)==4) voxelNMIGradientImage->datatype = NIFTI_TYPE_FLOAT32;
			else voxelNMIGradientImage->datatype = NIFTI_TYPE_FLOAT64;
			voxelNMIGradientImage->nbyper = sizeof(PrecisionTYPE);
			voxelNMIGradientImage->data = (void *)calloc(voxelNMIGradientImage->nvox, voxelNMIGradientImage->nbyper);
			nodeNMIGradientImage = nifti_copy_nim_info(controlPointImage);
			if(sizeof(PrecisionTYPE)==4) nodeNMIGradientImage->datatype = NIFTI_TYPE_FLOAT32;
			else nodeNMIGradientImage->datatype = NIFTI_TYPE_FLOAT64;
			nodeNMIGradientImage->nbyper = sizeof(PrecisionTYPE);
			nodeNMIGradientImage->data = (void *)calloc(nodeNMIGradientImage->nvox, nodeNMIGradientImage->nbyper);
			if(!flag->noConjugateGradient){
				conjugateG = (PrecisionTYPE *)calloc(nodeNMIGradientImage->nvox, sizeof(PrecisionTYPE));
				conjugateH = (PrecisionTYPE *)calloc(nodeNMIGradientImage->nvox, sizeof(PrecisionTYPE));
			}
			bestControlPointPosition = (PrecisionTYPE *)malloc(controlPointImage->nvox * sizeof(PrecisionTYPE));
			memcpy(bestControlPointPosition, controlPointImage->data, controlPointImage->nvox*controlPointImage->nbyper);
#ifdef _USE_CUDA
		}
#endif
		int smoothingRadius[3];
		smoothingRadius[0] = (int)floor( 2.0*controlPointImage->dx/targetImage->dx );
		smoothingRadius[1] = (int)floor( 2.0*controlPointImage->dy/targetImage->dy );
		smoothingRadius[2] = (int)floor( 2.0*controlPointImage->dz/targetImage->dz );

		int iteration=0;
        while(iteration<param->maxIteration && currentSize>smallestSize){
#ifdef _USE_CUDA
            if(flag->useGPUFlag){
                /* generate the position field */
                reg_bspline_gpu(controlPointImage,
                                targetImage,
                                &controlPointImageArray_d,
                                &positionFieldImageArray_d,
                                &targetMask_d,
                                activeVoxelNumber);
                /* Resample the source image */
                reg_resampleSourceImage_gpu(resultImage,
                                            sourceImage,
                                            &resultImageArray_d,
                                            &sourceImageArray_d,
                                            &positionFieldImageArray_d,
                                            &targetMask_d,
                                            activeVoxelNumber,
                                            param->sourceBGValue);
				/* The result image is transfered back to the host */
				if(cudaCommon_transferFromDeviceToNifti(resultImage, &resultImageArray_d)) return 1;
			}
			else{
#endif
				reg_bspline<PrecisionTYPE>(	controlPointImage,
                                            targetImage,
                                            positionFieldImage,
                                            targetMask,
                                            0);
				/* Resample the source image */
				reg_resampleSourceImage<PrecisionTYPE>(	targetImage,
                                                        sourceImage,
                                                        resultImage,
                                                        positionFieldImage,
                                                        targetMask,
                                                        1,
                                                        param->sourceBGValue);
#ifdef _USE_CUDA
			}
#endif

			double currentValue;
			reg_getEntropies<double>(	targetImage,
							resultImage,
							JH_PW_APPROX,
							param->binning,
							probaJointHistogram,
							logJointHistogram,
							entropies,
                            targetMask);
			currentValue = (1.0-param->bendingEnergyWeight-param->jacobianWeight)*(entropies[0]+entropies[1])/entropies[2];

#ifdef _USE_CUDA
			/* * NEED TO BE CHANGED */
			if(flag->useGPUFlag){
				/* both histogram are tranfered back to the device */
				float *tempB=(float *)malloc(param->binning*(param->binning+2)*sizeof(float));
				for(int i=0; i<param->binning*(param->binning+2);i++){
					tempB[i]=(float)logJointHistogram[i];
				}
				CUDA_SAFE_CALL(cudaMemcpy(logJointHistogram_d, tempB, param->binning*(param->binning+2)*sizeof(float), cudaMemcpyHostToDevice));
				free(tempB);
			}
#endif
			double currentWBE=0.0f;
			double currentWJac=0.0f;
#ifdef _USE_CUDA
			if(flag->useGPUFlag){
				if(flag->bendingEnergyFlag){
					currentWBE = param->bendingEnergyWeight
							* reg_bspline_ApproxBendingEnergy_gpu(	controlPointImage,
												&controlPointImageArray_d);
					currentValue -= currentWBE;
				}
				if(flag->jacobianWeightFlag){
					//TODO
				}
			}
			else{
#endif
				if(flag->bendingEnergyFlag){
					currentWBE = param->bendingEnergyWeight
							* reg_bspline_bendingEnergy<PrecisionTYPE>(controlPointImage, targetImage, flag->appBendingEnergyFlag);
					currentValue -= currentWBE;
				}
				if(flag->jacobianWeightFlag){
					if(flag->jacFullFlag){
						currentWJac = param->jacobianWeight
							* reg_bspline_jacobian<PrecisionTYPE>(controlPointImage, targetHeader, flag->appJacobianFlag);
					}
					else{
						currentWJac = param->jacobianWeight
							* reg_bspline_jacobian<PrecisionTYPE>(controlPointImage, targetImage, flag->appJacobianFlag);
					}
					currentValue -= currentWJac;
					if(currentWJac!=currentWJac) currentValue = 0.0f;
				}
#ifdef _USE_CUDA
			}
#endif

			double bestValue = currentValue;
			double bestWBE = currentWBE;
			double bestWJac = currentWJac;
			if(iteration==0){
				printf("Initial objective function value = %g\n", currentValue);

			}
			iteration++;
			float maxLength;

#ifdef _USE_CUDA
			if(flag->useGPUFlag){
				/* The NMI Gradient is calculated */
				reg_getSourceImageGradient_gpu(	targetImage,
								sourceImage,
								&sourceImageArray_d,
								&positionFieldImageArray_d,
								&resultGradientArray_d,
                                activeVoxelNumber);
				reg_getVoxelBasedNMIGradientUsingPW_gpu(targetImage,
									resultImage,
									&targetImageArray_d,
									&resultImageArray_d,
									&resultGradientArray_d,
									&logJointHistogram_d,
									&voxelNMIGradientArray_d,
                                    &targetMask_d,
                                    activeVoxelNumber,
									entropies,
									param->binning);
				reg_smoothImageForCubicSpline_gpu(  resultImage,
									                &voxelNMIGradientArray_d,
									                smoothingRadius);
				reg_voxelCentric2NodeCentric_gpu(   targetImage,
								                    controlPointImage,
								                    &voxelNMIGradientArray_d,
								                    &nodeNMIGradientArray_d);
				/* The NMI gradient is converted from voxel space to real space */
				reg_convertNMIGradientFromVoxelToRealSpace_gpu( sourceMatrix_xyz,
										                        controlPointImage,
										                        &nodeNMIGradientArray_d);
				/* The other gradients are calculated */
				if(flag->beGradFlag && flag->bendingEnergyFlag){
				    reg_bspline_ApproxBendingEnergyGradient_gpu(controlPointImage,
											                    &controlPointImageArray_d,
											                    &nodeNMIGradientArray_d,
											                    param->bendingEnergyWeight);
				}
				if(flag->jacGradFlag && flag->jacobianWeightFlag){
					//TODO
				}
				/* The conjugate gradient is computed */
				if(!flag->noConjugateGradient){
					if(iteration==1){
						// first conjugate gradient iteration
						reg_initialiseConjugateGradient(&nodeNMIGradientArray_d,
										&conjugateG_d,
										&conjugateH_d,
										controlPointImage->nx*controlPointImage->ny*controlPointImage->nz);
					}
					else{
						// conjugate gradient computation if iteration != 1
						reg_GetConjugateGradient(	&nodeNMIGradientArray_d,
										&conjugateG_d,
										&conjugateH_d,
										controlPointImage->nx*controlPointImage->ny*controlPointImage->nz);
					}
				}
				maxLength = reg_getMaximalLength_gpu(	&nodeNMIGradientArray_d,
									controlPointImage->nx*controlPointImage->ny*controlPointImage->nz);
			}
			else{
#endif
				/* The NMI Gradient is calculated */
				reg_getSourceImageGradient<PrecisionTYPE>(	targetImage,
										sourceImage,
										resultGradientImage,
										positionFieldImage,
                                        targetMask,
										1);
				reg_getVoxelBasedNMIGradientUsingPW<double>(targetImage,
                                                            resultImage,
                                                            JH_PW_APPROX,
                                                            resultGradientImage,
                                                            param->binning,
                                                            logJointHistogram,
                                                            entropies,
                                                            voxelNMIGradientImage,
                                                            targetMask);
				reg_smoothImageForCubicSpline<PrecisionTYPE>(voxelNMIGradientImage,smoothingRadius);
				reg_voxelCentric2NodeCentric(nodeNMIGradientImage,voxelNMIGradientImage);

				/* The NMI gradient is converted from voxel space to real space */
				PrecisionTYPE *gradientValuesX = static_cast<PrecisionTYPE *>(nodeNMIGradientImage->data);
				PrecisionTYPE *gradientValuesY = &gradientValuesX[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
				PrecisionTYPE *gradientValuesZ = &gradientValuesY[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
				PrecisionTYPE newGradientValueX, newGradientValueY, newGradientValueZ;
				for(int i=0; i<controlPointImage->nx*controlPointImage->ny*controlPointImage->nz; i++){
	
					newGradientValueX = 	*gradientValuesX * sourceMatrix_xyz->m[0][0] +
								*gradientValuesY * sourceMatrix_xyz->m[0][1] +
								*gradientValuesZ * sourceMatrix_xyz->m[0][2];
					newGradientValueY = 	*gradientValuesX * sourceMatrix_xyz->m[1][0] +
								*gradientValuesY * sourceMatrix_xyz->m[1][1] +
								*gradientValuesZ * sourceMatrix_xyz->m[1][2];
					newGradientValueZ = 	*gradientValuesX * sourceMatrix_xyz->m[2][0] +
								*gradientValuesY * sourceMatrix_xyz->m[2][1] +
								*gradientValuesZ * sourceMatrix_xyz->m[2][2];
	
					*gradientValuesX++ = newGradientValueX;
					*gradientValuesY++ = newGradientValueY;
					*gradientValuesZ++ = newGradientValueZ;
				}

				/* The other gradients are calculated */
				if(flag->beGradFlag && flag->bendingEnergyFlag){
                    reg_bspline_bendingEnergyGradient<PrecisionTYPE>(controlPointImage,
                                            targetImage,
                                            nodeNMIGradientImage,
                                            param->bendingEnergyWeight);
				}
				if(flag->jacGradFlag && flag->jacobianWeightFlag){
					if(flag->jacFullFlag){
						reg_bspline_jacobianDeterminantGradient<PrecisionTYPE>(	controlPointImage,
													targetHeader,//targetImage,
													nodeNMIGradientImage,
													param->jacobianWeight,
													flag->appJacobianFlag);
					}
					else{
						reg_bspline_jacobianDeterminantGradient<PrecisionTYPE>(	controlPointImage,
													targetImage,
													nodeNMIGradientImage,
													param->jacobianWeight,
													flag->appJacobianFlag);
					}
				}

				/* The conjugate gradient is computed */
				if(!flag->noConjugateGradient){
					if(iteration==1){
						// first conjugate gradient iteration
						PrecisionTYPE *conjGPtrX = &conjugateG[0];
						PrecisionTYPE *conjGPtrY = &conjGPtrX[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjGPtrZ = &conjGPtrY[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjHPtrX = &conjugateH[0];
						PrecisionTYPE *conjHPtrY = &conjHPtrX[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjHPtrZ = &conjHPtrY[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *gradientValuesX = static_cast<PrecisionTYPE *>(nodeNMIGradientImage->data);
						PrecisionTYPE *gradientValuesY = &gradientValuesX[nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz];
						PrecisionTYPE *gradientValuesZ = &gradientValuesY[nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz];
						for(int i=0; i<nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz;i++){
							*conjHPtrX++ = *conjGPtrX++ = - *gradientValuesX++;
							*conjHPtrY++ = *conjGPtrY++ = - *gradientValuesY++;
							*conjHPtrZ++ = *conjGPtrZ++ = - *gradientValuesZ++;
						}
					}
					else{
						double dgg=0.0, gg=0.0;
						PrecisionTYPE *conjGPtrX = &conjugateG[0];
						PrecisionTYPE *conjGPtrY = &conjGPtrX[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjGPtrZ = &conjGPtrY[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjHPtrX = &conjugateH[0];
						PrecisionTYPE *conjHPtrY = &conjHPtrX[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *conjHPtrZ = &conjHPtrY[nodeNMIGradientImage->nx * nodeNMIGradientImage->ny * nodeNMIGradientImage->nz];
						PrecisionTYPE *gradientValuesX = static_cast<PrecisionTYPE *>(nodeNMIGradientImage->data);
						PrecisionTYPE *gradientValuesY = &gradientValuesX[nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz];
						PrecisionTYPE *gradientValuesZ = &gradientValuesY[nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz];
						for(int i=0; i<nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz;i++){
							gg += conjHPtrX[i] * conjGPtrX[i];
							gg += conjHPtrY[i] * conjGPtrY[i];
							gg += conjHPtrZ[i] * conjGPtrZ[i];
							dgg += (gradientValuesX[i] + conjGPtrX[i]) * gradientValuesX[i];
							dgg += (gradientValuesY[i] + conjGPtrY[i]) * gradientValuesY[i];
							dgg += (gradientValuesZ[i] + conjGPtrZ[i]) * gradientValuesZ[i];
						}
						double gam = dgg/gg;
						for(int i=0; i<nodeNMIGradientImage->nx*nodeNMIGradientImage->ny*nodeNMIGradientImage->nz;i++){
							conjGPtrX[i] = - gradientValuesX[i];
							conjGPtrY[i] = - gradientValuesY[i];
							conjGPtrZ[i] = - gradientValuesZ[i];
							conjHPtrX[i] = (float)(conjGPtrX[i] + gam * conjHPtrX[i]);
							conjHPtrY[i] = (float)(conjGPtrY[i] + gam * conjHPtrY[i]);
							conjHPtrZ[i] = (float)(conjGPtrZ[i] + gam * conjHPtrZ[i]);
							gradientValuesX[i] = - conjHPtrX[i];
							gradientValuesY[i] = - conjHPtrY[i];
							gradientValuesZ[i] = - conjHPtrZ[i];
						}
					}
				}
				maxLength = reg_getMaximalLength<PrecisionTYPE>(nodeNMIGradientImage);
#ifdef _USE_CUDA
			}
#endif
	
			/* The gradient is applied to the control point positions */
			if(maxLength==0){
				printf("No Gradient ... exit\n");
				break;	
			}
#ifdef _VERBOSE
			printf("[VERBOSE] [%i] Max metric gradient value = %g\n", iteration, maxLength);
#endif
			
			/* ** LINE ASCENT ** */
			int lineIteration = 0;
			maxStepSize=currentSize;
			float addedStep=0.0f;
	
            while(currentSize>smallestSize && lineIteration<12){
				
				float currentLength = -currentSize/maxLength;

#ifdef _VERBOSE
				printf("[VERBOSE] [%i] Current added max step: %g\n", iteration, currentSize);
#endif
	
#ifdef _USE_CUDA
				if(flag->useGPUFlag){
					/* Update the control point position */
					reg_updateControlPointPosition_gpu(	controlPointImage,
										&controlPointImageArray_d,
										&bestControlPointPosition_d,
										&nodeNMIGradientArray_d,
										currentLength);


					/* generate the position field */
					reg_bspline_gpu(controlPointImage,
							targetImage,
							&controlPointImageArray_d,
							&positionFieldImageArray_d,
                            &targetMask_d,
                            activeVoxelNumber);
					/* Resample the source image */
					reg_resampleSourceImage_gpu(	resultImage,
									sourceImage,
									&resultImageArray_d,
									&sourceImageArray_d,
									&positionFieldImageArray_d,
                                    &targetMask_d,
                                    activeVoxelNumber,
									param->sourceBGValue);
					/* The result image is transfered back to the host */
					PrecisionTYPE *resultPtr=static_cast<PrecisionTYPE *>(resultImage->data);
					CUDA_SAFE_CALL(cudaMemcpy(resultPtr, resultImageArray_d, resultImage->nvox*sizeof(PrecisionTYPE), cudaMemcpyDeviceToHost));
				}
				else{
#endif
					/* Update the control point position */
					PrecisionTYPE *controlPointValuesX = static_cast<PrecisionTYPE *>(controlPointImage->data);
					PrecisionTYPE *controlPointValuesY = &controlPointValuesX[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					PrecisionTYPE *controlPointValuesZ = &controlPointValuesY[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					PrecisionTYPE *bestControlPointValuesX = &bestControlPointPosition[0];
					PrecisionTYPE *bestControlPointValuesY = &bestControlPointValuesX[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					PrecisionTYPE *bestControlPointValuesZ = &bestControlPointValuesY[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					PrecisionTYPE *gradientValuesX = static_cast<PrecisionTYPE *>(nodeNMIGradientImage->data);
					PrecisionTYPE *gradientValuesY = &gradientValuesX[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					PrecisionTYPE *gradientValuesZ = &gradientValuesY[controlPointImage->nx*controlPointImage->ny*controlPointImage->nz];
					for(int i=0; i<controlPointImage->nx*controlPointImage->ny*controlPointImage->nz;i++){
						*controlPointValuesX++ = *bestControlPointValuesX++ + currentLength * *gradientValuesX++;
						*controlPointValuesY++ = *bestControlPointValuesY++ + currentLength * *gradientValuesY++;
						*controlPointValuesZ++ = *bestControlPointValuesZ++ + currentLength * *gradientValuesZ++;
					}

					/* generate the position field */
					reg_bspline<PrecisionTYPE>(	controlPointImage,
									targetImage,
									positionFieldImage,
                                    targetMask,
									0);
					/* Resample the source image */
					reg_resampleSourceImage<PrecisionTYPE>(	targetImage,
										sourceImage,
										resultImage,
										positionFieldImage,
                                        NULL,
										1,
										param->sourceBGValue);
#ifdef _USE_CUDA
				}
#endif

				/* Computation of the NMI */
				reg_getEntropies<double>(	targetImage,
                                            resultImage,
                                            JH_PW_APPROX,
                                            param->binning,
                                            probaJointHistogram,
                                            logJointHistogram,
                                            entropies,
                                            targetMask);
				currentValue = (1.0-param->bendingEnergyWeight-param->jacobianWeight)*(entropies[0]+entropies[1])/entropies[2];

#ifdef _USE_CUDA
				if(flag->useGPUFlag){
					if(flag->bendingEnergyFlag){
						currentWBE = param->bendingEnergyWeight
								* reg_bspline_ApproxBendingEnergy_gpu(	controlPointImage,
													&controlPointImageArray_d);
						currentValue -= currentWBE;
					}
					if(flag->jacobianWeightFlag){
						//TODO
					}
				}
				else{
#endif
					if(flag->bendingEnergyFlag){
						currentWBE = param->bendingEnergyWeight
								* reg_bspline_bendingEnergy<PrecisionTYPE>(controlPointImage, targetImage, flag->appBendingEnergyFlag);
						currentValue -= currentWBE;
					}
					if(flag->jacobianWeightFlag){
						if(flag->jacFullFlag){
							currentWJac = param->jacobianWeight
								* reg_bspline_jacobian<PrecisionTYPE>(controlPointImage, targetHeader, flag->appJacobianFlag);
						}
						else{
							currentWJac = param->jacobianWeight
								* reg_bspline_jacobian<PrecisionTYPE>(controlPointImage, targetImage, flag->appJacobianFlag);
						}
						currentValue -= currentWJac;
						if(currentWJac!=currentWJac) currentValue = 0.0f;
					}
#ifdef _USE_CUDA
				}
#endif

#ifdef _VERBOSE
				printf("[VERBOSE] [%i] Current objective function value: %g\n", iteration, currentValue);
				if(flag->bendingEnergyFlag) printf("[VERBOSE] [%i] Weighted bending energy value = %g, approx[%i]\n", iteration, currentWBE, flag->appBendingEnergyFlag);
				if(flag->jacobianWeightFlag) printf("[VERBOSE] [%i] Weighted Jacobian log value = %g, approx[%i]\n", iteration, currentWJac, flag->appJacobianFlag);
#endif

				iteration++;
				lineIteration++;
		
				/* The current deformation field is kept if it was the best so far */
				if(currentValue>bestValue){
#ifdef _USE_CUDA
					if(flag->useGPUFlag){
						CUDA_SAFE_CALL(cudaMemcpy(bestControlPointPosition_d, controlPointImageArray_d,
							controlPointImage->nx*controlPointImage->ny*controlPointImage->nz*sizeof(float4),
							cudaMemcpyDeviceToDevice));
					}
					else{
#endif
						memcpy(bestControlPointPosition,controlPointImage->data,controlPointImage->nvox*controlPointImage->nbyper);
#ifdef _USE_CUDA
					}
#endif
					bestValue = currentValue;
					bestWBE = currentWBE;
					bestWJac = currentWJac;
					addedStep += currentSize;
					currentSize*=1.1f;
					currentSize = (currentSize<maxStepSize)?currentSize:maxStepSize;
				}
				else{
					currentSize*=0.5;
				}
			}
#ifdef _USE_CUDA
			if(flag->useGPUFlag){
				CUDA_SAFE_CALL(cudaMemcpy(controlPointImageArray_d, bestControlPointPosition_d,
					controlPointImage->nx*controlPointImage->ny*controlPointImage->nz*sizeof(float4),
					cudaMemcpyDeviceToDevice));
			}
			else{
#endif
				memcpy(controlPointImage->data,bestControlPointPosition,controlPointImage->nvox*controlPointImage->nbyper);
#ifdef _USE_CUDA
			}
#endif
			currentSize=addedStep;
			printf("[%i] Objective function value=%g | max. added disp. = %g mm", iteration, bestValue, addedStep);
			if(flag->bendingEnergyFlag) printf(" | wBE=%g", bestWBE);
			if(flag->jacobianWeightFlag) printf(" | wJacLog=%g", bestWJac);
			printf("\n");
		} // while(iteration<param->maxIteration && currentSize>smallestSize){
	
        free(targetMask);
		free(entropies);
		free(probaJointHistogram);
		free(logJointHistogram);
#ifdef _USE_CUDA
        if(flag->useGPUFlag){
            if(cudaCommon_transferFromDeviceToNifti(controlPointImage, &controlPointImageArray_d)) return 1;
            cudaCommon_free( (void **)&targetImageArray_d );
            cudaCommon_free( &sourceImageArray_d );
            cudaCommon_free( (void **)&controlPointImageArray_d );
            cudaCommon_free( (void **)&resultImageArray_d );
            cudaCommon_free( (void **)&positionFieldImageArray_d );
            CUDA_SAFE_CALL(cudaFree(targetMask_d));
            cudaCommon_free((void **)&resultGradientArray_d);
            cudaCommon_free((void **)&voxelNMIGradientArray_d);
            cudaCommon_free((void **)&nodeNMIGradientArray_d);
            if(!flag->noConjugateGradient){
                cudaCommon_free((void **)&conjugateG_d);
                cudaCommon_free((void **)&conjugateH_d);
            }
            cudaCommon_free((void **)&bestControlPointPosition_d);
            cudaCommon_free((void **)&logJointHistogram_d);

            CUDA_SAFE_CALL(cudaFreeHost(resultImage->data));
            resultImage->data = NULL;
        }
        else{
#endif
			nifti_image_free(resultGradientImage);
			nifti_image_free(voxelNMIGradientImage);
			nifti_image_free(nodeNMIGradientImage);
			if(!flag->noConjugateGradient){
				free(conjugateG);
				free(conjugateH);
			}
			free(bestControlPointPosition);
#ifdef _USE_CUDA
		}
#endif

        nifti_image_free( resultImage );

        if(level==(param->level2Perform-1)){
        /* ****************** */
        /* OUTPUT THE RESULTS */
        /* ****************** */

			/* The best result is returned */
            nifti_set_filenames(controlPointImage, param->outputCPPName, 0, 0);
			nifti_image_write(controlPointImage);

            if(param->level2Perform != param->levelNumber){
                free(positionFieldImage->data);
                positionFieldImage->dim[1]=positionFieldImage->nx=targetHeader->nx;
                positionFieldImage->dim[2]=positionFieldImage->ny=targetHeader->ny;
                positionFieldImage->dim[3]=positionFieldImage->nz=targetHeader->nz;
                positionFieldImage->dim[4]=positionFieldImage->nt=1;positionFieldImage->pixdim[4]=positionFieldImage->dt=1.0;
                positionFieldImage->dim[5]=positionFieldImage->nu=3;positionFieldImage->pixdim[5]=positionFieldImage->du=1.0;
                positionFieldImage->dim[6]=positionFieldImage->nv=1;positionFieldImage->pixdim[6]=positionFieldImage->dv=1.0;
                positionFieldImage->dim[7]=positionFieldImage->nw=1;positionFieldImage->pixdim[7]=positionFieldImage->dw=1.0;
                positionFieldImage->nvox=positionFieldImage->nx*positionFieldImage->ny*positionFieldImage->nz*positionFieldImage->nt*positionFieldImage->nu;
                positionFieldImage->data = (void *)calloc(positionFieldImage->nvox, positionFieldImage->nbyper);
            }

			/* The corresponding deformation field is evaluated and saved */
			reg_bspline<PrecisionTYPE>(	controlPointImage,
						                targetHeader,
						                positionFieldImage,
                                        NULL,
						                0);

            nifti_image_free( sourceImage );
            sourceImage = nifti_image_read(param->sourceImageName,true); // reload the source image with the correct intensity values

			resultImage = nifti_copy_nim_info(targetHeader);
			resultImage->cal_min = sourceImage->cal_min;
			resultImage->cal_max = sourceImage->cal_max;
			resultImage->scl_slope = sourceImage->scl_slope;
			resultImage->scl_inter = sourceImage->scl_inter;
			resultImage->datatype = sourceImage->datatype;
			resultImage->nbyper = sourceImage->nbyper;
			resultImage->data = (void *)calloc(resultImage->nvox, resultImage->nbyper);
			reg_resampleSourceImage<double>(targetHeader,
							                sourceImage,
							                resultImage,
							                positionFieldImage,
                                            NULL,
							                3,
							                param->sourceBGValue);
			if(!flag->outputResultFlag) param->outputResultName="outputResult.nii";
			nifti_set_filenames(resultImage, param->outputResultName, 0, 0);
			nifti_image_write(resultImage);
			nifti_image_free(resultImage);

		} // if(level==(param->levelNumber-1)){

		nifti_image_free( positionFieldImage );
		nifti_image_free( sourceImage );
		nifti_image_free( targetImage );

		printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n\n");
	} // for(int level=0; level<param->levelNumber; level++){

	/* Mr Clean */
	nifti_image_free( controlPointImage );
	nifti_image_free( targetHeader );
	nifti_image_free( sourceHeader );
    if(flag->targetMaskFlag) nifti_image_free( targetMaskImage );

	free( flag );
	free( param );

	time_t end; time( &end );
	int minutes = (int)floorf(float(end-start)/60.0f);
	int seconds = (int)(end-start - 60*minutes);
	printf("Registration Performed in %i min %i sec\n", minutes, seconds);
	printf("Have a good day !\n");

	return 0;
}

#endif
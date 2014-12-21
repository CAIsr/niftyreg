#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifndef INFODEVICE_H_
#define INFODEVICE_H_

#include "CLContextSingletton.h"


template<typename T>
class DeviceLog {
public:

	static void appendString(bool flag, std::string name, std::string & str) {
		if (flag) {
			if(str.length() > 0)  str.append(" / ") ;
			str.append(name);
		}
	}

	static void show(cl_device_id id, cl_device_info name, std::string str) {
		cl_int errNum;
		std::size_t paramValueSize;
		std::string clInfo;
		CLContextSingletton *sContext = &CLContextSingletton::Instance();
		const unsigned int outputWidth = 45;


		sContext->checkErrNum(clGetDeviceInfo(id, name, 0, NULL, &paramValueSize), "Failed to find OpenCL device info ");

		T * info = (T *) alloca(sizeof(T) * paramValueSize);
		sContext->checkErrNum(clGetDeviceInfo(id, name, paramValueSize, info, NULL), "Failed to find OpenCL device info ");

		switch (name) {
		case CL_DEVICE_TYPE: {
			const cl_device_type deviceType = *(reinterpret_cast<cl_device_type*>(info));
			appendString(deviceType & CL_DEVICE_TYPE_CPU, "CL_DEVICE_TYPE_CPU", clInfo);
			appendString(deviceType & CL_DEVICE_TYPE_GPU, "CL_DEVICE_TYPE_GPU", clInfo);
			appendString(deviceType & CL_DEVICE_TYPE_ACCELERATOR, "CL_DEVICE_TYPE_ACCELERATOR", clInfo);
			appendString(deviceType & CL_DEVICE_TYPE_DEFAULT, "CL_DEVICE_TYPE_DEFAULT", clInfo);

			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << clInfo << std::endl;
		}
			break;
		case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE: {
			const cl_device_mem_cache_type cacheType = *(reinterpret_cast<cl_device_mem_cache_type*>(info));
			appendString(cacheType & CL_NONE, "CL_NONE", clInfo);
			appendString(cacheType & CL_READ_ONLY_CACHE, "CL_READ_ONLY_CACHE", clInfo);
			appendString(cacheType & CL_READ_WRITE_CACHE, "CL_READ_WRITE_CACHE", clInfo);

			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << clInfo << std::endl;
		}
			break;
		case CL_DEVICE_LOCAL_MEM_TYPE: {
			const cl_device_local_mem_type localMemType = *(reinterpret_cast<cl_device_local_mem_type*>(info));
			appendString(localMemType & CL_LOCAL, "CL_LOCAL", clInfo);
			appendString(localMemType & CL_GLOBAL, "CL_GLOBAL", clInfo);

			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << clInfo << std::endl;
		}
			break;
		case CL_DEVICE_EXECUTION_CAPABILITIES: {

			const cl_device_exec_capabilities execCapabilities = *(reinterpret_cast<cl_device_exec_capabilities*>(info));

			appendString(execCapabilities & CL_EXEC_KERNEL, "CL_EXEC_KERNEL", clInfo);
			appendString(execCapabilities & CL_EXEC_NATIVE_KERNEL, "CL_EXEC_NATIVE_KERNEL", clInfo);

			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << clInfo << std::endl;
		}
			break;
		case CL_DEVICE_QUEUE_PROPERTIES: {

			appendString(*(reinterpret_cast<cl_device_exec_capabilities*>(info)) & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, "CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE", clInfo);
			appendString(*(reinterpret_cast<cl_device_exec_capabilities*>(info)) & CL_QUEUE_PROFILING_ENABLE, "CL_QUEUE_PROFILING_ENABLE", clInfo);

			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << clInfo << std::endl;
		}
			break;
		case CL_DEVICE_MAX_WORK_ITEM_SIZES: {
			cl_uint maxWorkItemDimensions;

			sContext->checkErrNum(clGetDeviceInfo(id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &maxWorkItemDimensions, NULL), "Failed to find OpenCL device info  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS.");
			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t";
			for (cl_uint i = 0; i < maxWorkItemDimensions; i++)
				std::cout << info[i] << " ";
			std::cout << std::endl;
		}
			break;

		case CL_DEVICE_NAME:
			case CL_DEVICE_VENDOR:
			case CL_DRIVER_VERSION:
			case CL_DEVICE_VERSION: {
			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << info << std::endl;
		}
			break;
		default:
			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << *info << std::endl;
			break;
		}
	}
	static void showKernelInfo(cl_device_id id, cl_kernel_work_group_info name, std::string str) {
		cl_int errNum;
		size_t local;
		CLContextSingletton *sContext = &CLContextSingletton::Instance();
		const unsigned int outputWidth = 45;

		errNum = clGetKernelWorkGroupInfo(sContext->dummyKernel(id), id, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(local), &local, NULL);

		switch (name) {
		case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
			if (errNum != CL_SUCCESS)  local = 1;
			std::cout << "\t"<< std::left<< std::setw(outputWidth) << "CL_WORK_GROUP_SIZE_MULTIPLE (WARP/WAVEFRONT)" << ":\t" << local << std::endl;
		}
			break;
			break;
		default:
			std::cout << "\t"<< std::left<< std::setw(outputWidth) << str << ":\t" << local << std::endl;
			break;
		}

	}
};

#endif /* INFODEVICE_H_ */

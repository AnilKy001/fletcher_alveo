#include "cmdlineparser.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <uuid/uuid.h>
#include <vector>
#include <xclhal2.h>
#include "expreimental/xclbin_util.h"
//Following is for stream APIs:
#include "CL/cl_ext_xilinx.h"
//Following is required for OpenCL C++ wrapper APIs
#include "xcl2.hpp"

//Declaration of custom streaming APIs from xcl2.hpp:
decltype(&clCreateStream) xcl::stream::createStream = nullptr;
decltype(&clReleaseStream) xcl::stream::releaseStream = nullptr;
decltype(&clReadStream) xcl::stream::readStream = nullptr;
decltype(&clWriteStream) xcl::stream::writeStream = nullptr;
decltype(&clPollStreams) xcl::stream::pollStreams = nullptr;
//decltype(&xclGetComputeUnitInfo) xcl::Ext::getComputeUnitInfo = nullptr;




fstatus_t platformInit(int argc, char **argv){


	std::string Block_Count = "1024";
	if(xcl::is_emulation()){
		Block_Count = "2";
	}

	//command line parser:
	sda::utils::CmdLineParser parser;

	//Switches:
	parser.addSwitch("--xclbin_file", "-x", "input binary file string: ", "");
	parser.addSwitch("--block_count", "-nb", "number of blocks", Block_Count);
	parser.addSwitch("--block_size", "-bs", "Size of Each Block in KB", "256");
	parser.parse(argc, argv);

	//Read settings:
	std::string binaryFile = parser.value("xclbin_file");
	unsigned int num_Blocks = stoi(parser.value("block_count"));
	unsigned int BLock_Size = stoi(parser.value("block_size"));

	//If no xclbin file is provided, exit.
	if(binaryFile.empty()){
		parser.printHelp();
		exit();
	}

	//A vector of available devices:
	auto devices = xcl::get_xil_devices();
	
	//Load the binary file and return the pointer 
	//to file buffer.
	auto fileBuf = xcl::read_binary_file(binaryFile);
	alveo_state.bins{{fileBuf.data(), fleBuf.size()}};
	bool valid_device = false;
	for(unsigned int = 0; i < devices.size(); i++){
		alveo_state.device = devices[i];
		OCL_CHECK(alveo_state.err, alveo_state.context = cl::Context(
					device, NULL, NULL, NULL, &alveo_state.err));
		OCL_CHECK(alveo_state.err, alveo_state.q = cl::CommandQueue(
					alveo_state.context, alveo_state.device, 
					CL_QUEUE_PROFILING_ENABLE | 
					CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &alveo_state.err));

		//Now, program the device.
		std::cout << "Trying to program device[" << i << "]: " << alveo_state.device.getInfo<
			CL_DEVICE_NAME>() << std::endl;

		alveo_state.program(alveo_state.context, {alveo_state.device}, alveo_state.bins, 
				NULL, &alveo_state.err);

		if(alveo_state.err != CL_SUCCESS){
			std::cout << "Failed to program the device [" << i << "] with xclbin file!\n ";
		}
		else{
			std::cout << "Device [" << i << "]: programming successful!\n";


		//Creating Kernel:
		OCL_CHECK(alveo_state.err, alveo_state.increment = cl::Kernel(alveo_state.program, 
					"increment", &alveo_state.err));
		valid_device = true;
		break; //we break because we found a valid device.
		}
	}






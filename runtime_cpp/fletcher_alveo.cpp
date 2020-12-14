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


void performStream(){

	//Initialization of the streaming class before usage is needed before usage.
	xcl::Stream::init(alveo_state.platform_id);
	xcl::Ext::init(alveo_state.platform_id);

	cl_int ret;

	// Device Connection specification of the stream through extension pointer:
  cl_mem_ext_ptr_t ext_stream;
  ext_stream.param = (alveo_state.kernel).get();
  ext_stream.obj = NULL;

	// The .flag should be used to denote the kernel argument
	// Create write stream for argument 1 of kernel:
  ext_stream.flags = 1;

  // Create streams:
  cl_stream axis00_stream, axis01_stream;
  OCL_CHECK(ret, axis00_stream = xcl::Stream::createStream(
                     (alveo_state.device).get(), XCL_STREAM_WRITE_ONLY, CL_STREAM,
                     &ext_stream, &ret));

  ext_stream.flags = 0;
  OCL_CHECK(ret, axis01_stream = xcl::Stream::createStream(
                     (alveo_state.device).get(), XCL_STREAM_READ_ONLY, CL_STREAM,
										 &ext_stream, &ret));

  // Initiating the WRITE transfer
  cl_stream_xfer_req wr_req{0};
  wr_req.flags = CL_STREAM_EOT;
  OCL_CHECK(ret, xcl::Stream::writeStream(axis01_stream, h_data.data(),
                                          vector_size_bytes, &wr_req, &ret));

  // Initiating the READ transfer
  cl_stream_xfer_req rd_req{0};
  rd_req.flags = CL_STREAM_EOT;
  OCL_CHECK(ret, xcl::Stream::readStream(axis00_stream, read_data.data(),
                                         vector_size_bytes, &rd_req, &ret));
}

fstatus_t platformInit(int argc, char **argv){

	//command line parser:
	sda::utils::CmdLineParser parser;

	//Get the .xclbin file from the terminal command:
	parser.addSwitch("--xclbin_file", "-x", "input binary file string: ", "");
	parser.parse(argc, argv);

	//Read settings:
	std::string binaryFile = parser.value("xclbin_file");

	//If no xclbin file is provided, exit.
	if(binaryFile.empty()){
		parser.printHelp();
		exit();
	}


	//A vector of available devices:
	auto devices = xcl::get_xil_devices();

	//Load the binary file and return the pointer
	//to file buffer.
	alveo_state.fileBuf = xcl::read_binary_file(binaryFile);
	alveo_state.bins{{(alveo_state.fileBuf).data(), fleBuf.size()}};
	bool valid_device = false;
	for(unsigned int = 0; i < devices.size(); i++){
		alveo_state.device = devices[i];
		//Create a Context and a Command Queue for the selected device.
		OCL_CHECK(alveo_state.err, alveo_state.context = cl::Context(
					alveo_state.device, NULL, NULL, NULL, &alveo_state.err));
		OCL_CHECK(alveo_state.err, alveo_state.q = cl::CommandQueue(
					alveo_state.context, alveo_state.device,
					CL_QUEUE_PROFILING_ENABLE |
					CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &(alveo_state.err)));

		//Now, program the device.
		std::cout << "Trying to program device[" << i << "]: " << (alveo_state).device.getInfo<
			CL_DEVICE_NAME>() << std::endl;

		alveo_state.program(alveo_state.context, {alveo_state.device}, alveo_state.bins,
				NULL, &alveo_state.err);

		if(alveo_state.err != CL_SUCCESS){
			std::cout << "Failed to program the device [" << i << "] with xclbin file!\n ";
		}
		else{
			std::cout << "Device [" << i << "]: programming successful!\n";


		//Creating Kernel (contained within .xlcbin file/Program):
		OCL_CHECK(alveo_state.err, alveo_state.kernel = cl::Kernel(alveo_state.program,
					"kernel", &alveo_state.err));
		valid_device = true;
		break; //we break because we found a valid device.
		}
	}

	if(!valid_device){
		std::cout << "Failed to program any device, exiting." << std::endl;
		exit(EXIT_FAILURE);
	}

	platform_id = (alveo_state.device).getInfo<CL_DEVICE_PLATFORM>(&(alveo_state.err));
}



fstatus_t platformCopyHostToDevice(const uint8_t *host_source, da_t device_destination, int64_t size){



	//Streams are:
	cl_stream h2c_stream_a;
	cl_stream c2h_stream;

	cl_int ret;

	// Device Connection specification of the stream through extension pointer:
	cl_mem_ext_ptr_t ext;
	ext.param = increment.get();
	ext.obj = NULL;

	// The .flag should be used to denote the kernel argument
	// Create write stream for argument 0 of kernel:
	ext.flags = 0;

	OCL_CHECK(ret, h2c_stream_a = xcl::Stream::createStream((alveo_state.device).get(),
			XCL_STREAM_READ_ONLY, CL_STREAM, &(alveo_state.ext), &(alveo_state.ret)));

	// Create read stream for argument 1 of kernel:
  	ext.flags = 1;
  	OCL_CHECK(ret, c2h_Stream = xcl::Stream::createStream((alveo_state.device).get(),
                       	XCL_STREAM_WRITE_ONLY,CL_STREAM, &(alveo_state.ext), &(alveo_state.ret)));

	//Sync for the async streaming.
	int num_compl = 2 * num_Blocks;

	//Checking the request completion:
	cl_streams_poll_req_completions *poll_req;
	poll_req = (cl_streams_poll_req_completions *)malloc(sizeof(
				cl_streams_poll_req_completion)*num_compl);



fstatus_t platformWriteMMIO(uint64_t offset, uint32_t value){
	xcl::Stream::init(alveo_state.platform_id);
	xcl::Ext::init(alveo_state.platform_id);

	uuid_t xclbinId;
	xclbin_uuid((alveo_state.fileBuf).data(), xclbinId);

	//Getting the device handle:
	clGetDeviceInfo((alveo_state.device).get(), CL_DEVICE_HANDLE,
		sizeof(alveo_state.handle), &(alveo_state.handle), nullptr);

	cl_uint cuidx;
	xcl::Ext::getComputeUnitInfo((alveo_state.kernel).get(), 0,
		XCL_COMPUTE_UNIT_INDEX, sizeof(cuidx), &cuidx, nullptr);

	//Write the register value:
	xclOpenContext(alveo_state.handle, xclbinId, cuidx, false);

	//Write "value" to "offset":
	xclRegWrite(alveo_state.handle, cuidx, offset, value);
	std::cout << "\nThe register value that is written: " << std::endl;

	uint32_t drive_valid = 1;
	xclRegWrite(alveo_state.handle, cuidx, offset + sizeof(int), drive_valid);

	//Closing the context:
	xclCloseContext(alveo_state.handle, xclbinId, cuidx);


	//Streaming:
	cl_int ret;
	cl_mem_ext_ptr_t ext_stream;
	ext_stream.param = krnl_incr.get();
	ext_stream.obj = NULL;
	ext_stream.flags = 1;





}

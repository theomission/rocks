#include <iostream>
#include <memory>
#include <fstream>
#include "common.hh"
#include "commonmath.hh"
#include "compute.hh"
#include "menu.hh"

////////////////////////////////////////////////////////////////////////////////
// TODO technically this should support multiple devices, but until I have 
// hardware like that, this only takes one
class ComputeContext
{
public:
	ComputeContext(cl_device_id device, bool shareGl);
	~ComputeContext();
	std::string GetDeviceName() const ;

	bool m_valid;
	cl_device_id m_device;
	cl_context m_context;
	cl_command_queue m_queue;

private:
	static void OnError(const char* errinfo, 
		const void *private_info, size_t cb, void* user_data);
};
	
void ComputeContext::OnError(const char* errinfo, 
		const void *private_info, size_t cb, void* user_data)
{
	std::cerr << "OpenCL error: " << errinfo << std::endl;
}
	
ComputeContext::ComputeContext(cl_device_id device, bool shareGl)
	: m_valid(false)
	, m_device(device)
	, m_context(0)
	, m_queue(0)
{
	cl_int ret;
	if(shareGl) { 
		std::cerr << "opengl sharing not really implemented yet." << std::endl;
	};
	//const cl_context_properties properties[] = {
	//	CL_GLX_DISPLAY_KHR,
	//	0,		// attribute list

	//	0			// term
	//};
	m_context = clCreateContext(nullptr, 1, &device, OnError, nullptr, &ret);

	compute_CheckError(ret, "clCreateContext");
	if(!m_context) return;
	m_queue = clCreateCommandQueue(m_context, m_device, 0, &ret);
	compute_CheckError(ret, "clCreateCommandQueue");
	if(!m_queue) return;
	m_valid = true;
}

ComputeContext::~ComputeContext()
{
	if(m_queue)
		clReleaseCommandQueue(m_queue);
	if(m_context)
		clReleaseContext(m_context);
}
	
std::string ComputeContext::GetDeviceName() const 
{
	char str[256] = {};
	clGetDeviceInfo(m_device, CL_DEVICE_NAME, sizeof(str), str, nullptr);
	return str;
}

////////////////////////////////////////////////////////////////////////////////
// forward decls
static void compute_SetCurrentDevice(cl_device_id id);

////////////////////////////////////////////////////////////////////////////////
// file-scope globals
static std::shared_ptr<ComputeContext> g_context;
static std::shared_ptr<ComputeProgram> g_prefixSumProgram;

////////////////////////////////////////////////////////////////////////////////
// util

static const char* compute_GetErrorStr(cl_int err)
{
	switch (err) {
		case CL_SUCCESS:                            return "Success!";
		case CL_DEVICE_NOT_FOUND:                   return "Device not found.";
		case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
		case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
		case CL_OUT_OF_RESOURCES:                   return "Out of resources";
		case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
		case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
		case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
		case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
		case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
		case CL_MAP_FAILURE:                        return "Map failure";
		case CL_INVALID_VALUE:                      return "Invalid value";
		case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
		case CL_INVALID_PLATFORM:                   return "Invalid platform";
		case CL_INVALID_DEVICE:                     return "Invalid device";
		case CL_INVALID_CONTEXT:                    return "Invalid context";
		case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
		case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
		case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
		case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
		case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
		case CL_INVALID_SAMPLER:                    return "Invalid sampler";
		case CL_INVALID_BINARY:                     return "Invalid binary";
		case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
		case CL_INVALID_PROGRAM:                    return "Invalid program";
		case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
		case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
		case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
		case CL_INVALID_KERNEL:                     return "Invalid kernel";
		case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
		case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
		case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
		case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
		case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
		case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
		case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
		case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
		case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
		case CL_INVALID_EVENT:                      return "Invalid event";
		case CL_INVALID_OPERATION:                  return "Invalid operation";
		case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
		case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
		case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
		default: return "Unknown";
	}
}

void compute_CheckError(cl_int ret, const char* name)
{
	if(ret != CL_SUCCESS)
	{
		std::cerr << name << ": " << compute_GetErrorStr(ret) << std::endl;
	}
}

////////////////////////////////////////////////////////////////////////////////
void compute_Init(const char* requestedDevice)
{
	if(requestedDevice && *requestedDevice)
	{
		cl_device_id id = compute_FindDeviceByName(requestedDevice);
		if(id)
			compute_SetCurrentDevice(id);
		if(!g_context) { 
			std::cout << "Failed to find device " << requestedDevice << ", searching..." 
				<< std::endl; 
		}
	}

	// on fall through or fail...
	if(!g_context)
	{
		// find the first device of the first platform
		auto platforms = compute_GetPlatforms();
		if(platforms.empty())
		{
			std::cerr << "No OpenCL platforms found." << std::endl;
			return;
		}

		for(const auto& plat: platforms)
		{
			auto devices = plat.GetDevices();
			for(const auto& dev: devices) {
				compute_SetCurrentDevice(dev.m_id);
				if(g_context)
				{
					std::cout << "Found compute device: " << dev.m_name << std::endl;
					break;
				}
			}
		}

	}

	if(!g_context)
		std::cerr << "compute_Init() failed." << std::endl;
	else
	{
		g_prefixSumProgram = compute_CompileProgram("programs/prefixsum.cl");
	}
}

cl_device_id compute_FindDeviceByName(const char* name)
{
	auto platforms = compute_GetPlatforms();
	for(const auto& plat: platforms)
	{
		auto devices = plat.GetDevices();
		for(const auto& dev: devices) {
			if(strcasecmp(name, dev.m_name.c_str()) == 0)
				return dev.m_id;
		}
	}
	return 0;
}

std::shared_ptr<SubmenuMenuItem> compute_CreateDeviceMenu()
{
	auto menu = std::make_shared<SubmenuMenuItem>("device");
	auto platforms = compute_GetPlatforms();
	for(const auto& plat: platforms) {
		auto platMenu = std::make_shared<SubmenuMenuItem>(plat.m_name);
		menu->AppendChild(platMenu);
		auto devices = plat.GetDevices();
		for(const auto& dev: devices) {
			cl_device_id id = dev.m_id;
			platMenu->AppendChild(
				std::make_shared<ButtonMenuItem>(dev.m_name, 
					[id]()
					{ compute_SetCurrentDevice(id); }));
		}
	}
	return menu;
}

static void compute_SetCurrentDevice(cl_device_id id)
{
	g_context.reset();
	g_context = std::make_shared<ComputeContext>(id, false);
	if(!g_context->m_valid)
		g_context.reset();
}

std::string compute_GetCurrentDeviceName()
{
	if(g_context) {
		return g_context->GetDeviceName();
	} else return "";
}

std::vector<ComputePlatform> compute_GetPlatforms()
{
	cl_uint numPlatforms;
	clGetPlatformIDs(0, nullptr, &numPlatforms);
	if(numPlatforms == 0)
	{
		std::cerr << "no OpenCL platforms found." << std::endl;
		return {};
	}
	
	std::vector<cl_platform_id> platforms(numPlatforms);
	clGetPlatformIDs(numPlatforms, &platforms[0], nullptr);

	std::vector<ComputePlatform> result;
	for(cl_platform_id id: platforms)
		result.emplace_back(id);
	return result;
}

std::shared_ptr<ComputeProgram> compute_CompileProgram(const char* filename)
{
	if(!g_context) return nullptr;
	return std::make_shared<ComputeProgram>(g_context->m_context, filename);
}

////////////////////////////////////////////////////////////////////////////////
ComputeDevice::ComputeDevice(cl_device_id id)
	: m_id(id)
{
	char str[256] = {};
	clGetDeviceInfo(id, CL_DEVICE_NAME, sizeof(str), str, nullptr);
	m_name = str;
	clGetDeviceInfo(id, CL_DEVICE_TYPE, sizeof(m_type), &m_type, nullptr);
	clGetDeviceInfo(id, CL_DEVICE_VENDOR, sizeof(str), str, nullptr);
	m_vendor = str;
}
	
std::string ComputeDevice::GetTypeStr() const
{
	std::string result;
	if(m_type & CL_DEVICE_TYPE_CPU)
		result += "CPU ";
	if(m_type & CL_DEVICE_TYPE_GPU)
		result += "GPU ";
	if(m_type & CL_DEVICE_TYPE_ACCELERATOR)
		result += "ACCEL ";
	if(m_type & CL_DEVICE_TYPE_DEFAULT)
		result += "DEF ";
	return result;
}

////////////////////////////////////////////////////////////////////////////////
ComputePlatform::ComputePlatform(cl_platform_id id)
	: m_id(id)
{
	char str[256]={};
	clGetPlatformInfo(id, CL_PLATFORM_PROFILE, sizeof(str), str, nullptr);
	m_profile = str;
	clGetPlatformInfo(id, CL_PLATFORM_VERSION, sizeof(str), str, nullptr);
	m_version = str;
	clGetPlatformInfo(id, CL_PLATFORM_NAME, sizeof(str), str, nullptr);
	m_name = str;
	clGetPlatformInfo(id, CL_PLATFORM_VENDOR, sizeof(str), str, nullptr);
	m_vendor = str;
}

std::vector<ComputeDevice> ComputePlatform::GetDevices(cl_device_type type) const
{
	cl_uint count;
	clGetDeviceIDs(m_id, type, 0, nullptr, &count);
	if(!count) return {};

	std::vector<cl_device_id> devices(count);
	clGetDeviceIDs(m_id, type, count, &devices[0], nullptr);

	std::vector<ComputeDevice> result;
	for(cl_device_id devid: devices)
		result.emplace_back(devid);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
ComputeProgram::ComputeProgram(cl_context ctx, const char* filename)
	: m_filename(filename)
	, m_program(0)
	, m_context(ctx)
{
	Recompile();
}
	
void ComputeProgram::Recompile()
{
	std::ifstream file(m_filename.c_str(), std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if(!file) {
		std::cerr << "Failed to open " << m_filename << std::endl;
		return;
	}
	size_t fileSize = file.tellg();
	file.seekg(0);
	std::vector<char> data(fileSize);
	file.read(&data[0], fileSize);

	if(file.fail()) {
		std::cerr << "failed to read " << m_filename << std::endl;
		return;
	}

	cl_int ret;
	if(m_program) 
		clReleaseProgram(m_program);
	m_program = clCreateProgramWithSource(m_context, 1, 
		(const char*[]){&data[0]}, 
		(const size_t[]){data.size()}, 
		&ret);
	compute_CheckError(ret, "clCreateProgramWithSource");
	ret = clBuildProgram(m_program, 0, nullptr, "-Iprograms", nullptr, nullptr );
	compute_CheckError(ret, "clBuildProgram");
	if(ret != CL_SUCCESS)
	{
		cl_uint numDevices;
		clGetProgramInfo(m_program, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
		std::vector<cl_device_id> devices(numDevices);
		clGetProgramInfo(m_program, CL_PROGRAM_DEVICES, numDevices * sizeof(cl_device_id), &devices[0], nullptr);
		for(auto dev: devices)
		{
			cl_build_status status;
			clGetProgramBuildInfo(m_program, dev, CL_PROGRAM_BUILD_STATUS, sizeof(status), &status, nullptr);
			if(status == CL_BUILD_ERROR)
			{
				char str[256] = {};
				clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof(str), str, nullptr);

				size_t logSize ;
				clGetProgramBuildInfo(m_program, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
				std::vector<char> log(logSize);
				clGetProgramBuildInfo(m_program, dev, CL_PROGRAM_BUILD_LOG, logSize, &log[0], nullptr);

				std::cerr << "Build error for " << m_filename << " on device " << str << std::endl
					<< &log[0] << std::endl;
			}
		}
	}
}

ComputeProgram::~ComputeProgram()
{
	if(m_program)
		clReleaseProgram(m_program);
}

std::shared_ptr<ComputeKernel> ComputeProgram::CreateKernel(const char* kernelName)
{
	return std::make_shared<ComputeKernel>(m_program, kernelName);
}

////////////////////////////////////////////////////////////////////////////////
ComputeKernel::ComputeKernel(cl_program prg, const char* kernelName)
	: m_kernel(0)
{
	cl_int err;
	m_kernel = clCreateKernel(prg, kernelName, &err);
	compute_CheckError(err, "clCreateKernel");
}

ComputeKernel::~ComputeKernel()
{
	if(m_kernel)
		clReleaseKernel(m_kernel);
}
	
void ComputeKernel::SetArg(int index, const ComputeBuffer* buffer) const
{
	cl_int ret = clSetKernelArg(m_kernel, index, sizeof(buffer->m_mem), &buffer->m_mem);
	compute_CheckError(ret, "clSetKernelArg");
}

void ComputeKernel::SetArg(int index, const ComputeImage* image) const
{
	cl_int ret = clSetKernelArg(m_kernel, index, sizeof(image->m_mem), &image->m_mem);
	compute_CheckError(ret, "clSetKernelArg");
}

void ComputeKernel::SetArg(int index, size_t arg_size, const void* value) const
{
	cl_int ret = clSetKernelArg(m_kernel, index, arg_size, value);
	compute_CheckError(ret, "clSetKernelArg");
}
	
void ComputeKernel::SetArgTempSize(int index, size_t size) const
{
	cl_int ret = clSetKernelArg(m_kernel, index, size, nullptr);
	compute_CheckError(ret, "clSetKernelArg");
}
	
void ComputeKernel::Enqueue(cl_uint dims, const size_t* globalWorkSize, const size_t* localWorkSize,
	cl_uint numEvents, const cl_event* events) const
{
	if(!m_kernel)
	{
		std::cerr << "invalid kernel" << std::endl;
		return;
	}
	cl_int ret = clEnqueueNDRangeKernel(g_context->m_queue,
		m_kernel,
		dims,
		nullptr,
		globalWorkSize,
		localWorkSize,
		numEvents,
		events,
		nullptr);
	compute_CheckError(ret, "clEnqueueNDRangeKernel");
}

ComputeEvent ComputeKernel::EnqueueEv(cl_uint dims, const size_t* globalWorkSize, const size_t *localWorkSize,
	cl_uint numEvents, const cl_event* events) const
{
	if(!m_kernel)
	{
		std::cerr << "invalid kernel" << std::endl;
		return 0;
	}
	cl_event event = {};
	cl_int ret = clEnqueueNDRangeKernel(g_context->m_queue,
		m_kernel,
		dims,
		nullptr,
		globalWorkSize,
		localWorkSize,
		numEvents,
		events,
		&event);
	compute_CheckError(ret, "clEnqueueNDRangeKernel");
	return event;
}

void ComputeKernel::Enqueue(cl_uint dims, 
	const size_t* globalWorkOffset,
	const size_t* globalWorkSize,
	const size_t* localWorkSize,
	cl_uint numEvents, const cl_event* events) const
{
	if(!m_kernel)
	{
		std::cerr << "invalid kernel" << std::endl;
		return;
	}
	cl_int ret = clEnqueueNDRangeKernel(g_context->m_queue,
		m_kernel,
		dims,
		globalWorkOffset,
		globalWorkSize,
		localWorkSize,
		numEvents,
		events,
		nullptr);
	compute_CheckError(ret, "clEnqueueNDRangeKernel");
}

ComputeEvent ComputeKernel::EnqueueEv(cl_uint dims,
	const size_t* globalWorkOffset,
	const size_t* globalWorkSize, 
	const size_t *localWorkSize,
	cl_uint numEvents, const cl_event* events) const
{
	if(!m_kernel)
	{
		std::cerr << "invalid kernel" << std::endl;
		return 0;
	}
	cl_event event = {};
	cl_int ret = clEnqueueNDRangeKernel(g_context->m_queue,
		m_kernel,
		dims,
		globalWorkOffset,
		globalWorkSize,
		localWorkSize,
		numEvents,
		events,
		&event);
	compute_CheckError(ret, "clEnqueueNDRangeKernel");
	return event;
}


////////////////////////////////////////////////////////////////////////////////
ComputeBuffer::ComputeBuffer(cl_context ctx, cl_mem_flags flags, size_t size, void* ptr)
	: m_mem(0)
{
	cl_int ret;
	m_mem = clCreateBuffer(ctx, flags, size, ptr, &ret);
	compute_CheckError(ret, "clCreateBuffer");
}

ComputeBuffer::~ComputeBuffer()
{
	if(m_mem) clReleaseMemObject(m_mem);
}

ComputeEvent ComputeBuffer::EnqueueRead(size_t offset, size_t cb, void* hostMem,
	cl_uint numEvents, const cl_event* events) const
{
	cl_event event = {};
	if(!m_mem) {
		std::cerr << "EnqueueRead on invalid buffer" << std::endl;
		return 0;
	}
	cl_int ret = clEnqueueReadBuffer(g_context->m_queue, m_mem, CL_FALSE,
		offset, cb, hostMem, numEvents, events, &event);
	compute_CheckError(ret, "clEnqueueReadBuffer");
	return event;
}

ComputeEvent ComputeBuffer::EnqueueWrite(size_t offset, size_t cb, const void* hostMem,
	cl_uint numEvents, const cl_event* events) const
{
	cl_event event = {};
	if(!m_mem) {
		std::cerr << "EnqueueRead on invalid buffer" << std::endl;
		return 0;
	}
	cl_int ret = clEnqueueWriteBuffer(g_context->m_queue, m_mem, CL_FALSE,
		offset, cb, hostMem, numEvents, events, &event);
	compute_CheckError(ret, "clEnqueueWriteBuffer");
	return event;
}

////////////////////////////////////////////////////////////////////////////////
//ComputeImage::ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format,
//	const cl_image_desc *image_desc, void* ptr)
//: m_mem(0)
//{
//	m_mem = clCreateImage(ctx, flags, image_format, image_desc, ptr, nullptr);
//}

ComputeImage::ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format, 
	size_t width, size_t height, 
	size_t pitch, void* ptr)
	: m_mem(0)
{
	cl_int err;
	m_mem = clCreateImage2D(ctx, flags, image_format, width, height, pitch, ptr, &err);
	compute_CheckError(err, "clCreateImage2D");
}
	
ComputeImage::ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format, 
			size_t width, size_t height, size_t depth,
			size_t rowPitch, size_t slicePitch, void* ptr)
{
	cl_int err;
	m_mem = clCreateImage3D(ctx, flags, image_format, width, height, depth, rowPitch, slicePitch, ptr, &err);
	compute_CheckError(err, "clCreateImage2D");
}

ComputeImage::ComputeImage(cl_context ctx, cl_mem_flags flags, GLenum target, GLuint texture)
	: m_mem(0)
{
	cl_int err;
	if(target == GL_TEXTURE_3D) {
		m_mem = clCreateFromGLTexture3D(ctx, flags, target, 0, texture, &err);
		compute_CheckError(err, "clCreateFromGLTexture3D");
	} else {
		m_mem = clCreateFromGLTexture2D(ctx, flags, target, 0, texture, &err);
		compute_CheckError(err, "clCreateFromGLTexture2D");
	}
}

ComputeImage::~ComputeImage()
{
	if(m_mem) clReleaseMemObject(m_mem);
}
	
ComputeEvent ComputeImage::EnqueueRead(const size_t origin[3], const size_t region[3], void* ptr)
{
	if(!m_mem) {
		std::cerr << "Invalid image" << std::endl;
		return 0;
	}
	cl_event event = {};
	cl_int ret = clEnqueueReadImage(g_context->m_queue, 
		m_mem,
		CL_FALSE,
		origin,
		region,
		0, 0,
		ptr,
		0, nullptr,
		&event);
	compute_CheckError(ret, "clEnqueueReadImage");
	return event;
}

////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<ComputeBuffer> compute_CreateBufferRO(size_t size, const void* hostData)
{
	if(!g_context) return nullptr;
	cl_mem_flags flags = CL_MEM_READ_ONLY;
	if(hostData)
		flags |= CL_MEM_COPY_HOST_PTR;
	// note: const_cast used here because this buffer is created by copying the host mem ptr, not
	// using it. OpenCL only has the one create buffer function and it takes a non-const ptr.
	return std::make_shared<ComputeBuffer>(g_context->m_context, 
		flags, size, const_cast<void*>(hostData));
}

std::shared_ptr<ComputeBuffer> compute_CreateBufferWO(size_t size) 
{
	if(!g_context) return nullptr;
	return std::make_shared<ComputeBuffer>(g_context->m_context, 
		CL_MEM_WRITE_ONLY, size, nullptr);
}

std::shared_ptr<ComputeBuffer> compute_CreateBufferRW(size_t size, const void* hostData) 
{
	if(!g_context) return nullptr;
	cl_mem_flags flags = CL_MEM_READ_WRITE;
	if(hostData)
		flags |= CL_MEM_COPY_HOST_PTR;
	// note: const_cast used here because this buffer is created by copying the host mem ptr, not
	// using it. OpenCL only has the one create buffer function and it takes a non-const ptr.
	return std::make_shared<ComputeBuffer>(g_context->m_context, 
		flags, size, const_cast<void*>(hostData));
}

std::shared_ptr<ComputeImage> compute_CreateImage2DWO(size_t width, size_t height, 
	cl_channel_order ord, cl_channel_type type)
{
	if(!g_context) return nullptr;

	return std::make_shared<ComputeImage>(g_context->m_context,
		CL_MEM_WRITE_ONLY, 
		(const cl_image_format[]){{ord, type}}, 
		width,
		height,
		0,
		nullptr);
}

std::shared_ptr<ComputeImage> compute_CreateImage3DWO(size_t width, size_t height, size_t depth,
	cl_channel_order ord, cl_channel_type type)
{
	if(!g_context) return nullptr;

	return std::make_shared<ComputeImage>(g_context->m_context,
		CL_MEM_WRITE_ONLY, 
		(const cl_image_format[]){{ord, type}}, 
		width,
		height,
		depth,
		0,
		0,
		nullptr);
}

std::shared_ptr<ComputeImage> compute_CreateImageFromGLWO(GLenum target, GLuint tex)
{
	if(!g_context) return nullptr;
	return std::make_shared<ComputeImage>(g_context->m_context, CL_MEM_WRITE_ONLY,
		target, tex);
}

void compute_WaitForEvent(const ComputeEvent& event)
{
	if(event.m_event)
		clWaitForEvents(1, (const cl_event[]){event.m_event});
	else 
		std::cerr << "waiting for null event!" << std::endl;
}

void compute_EnqueueWaitForEvent(const ComputeEvent& event)
{
	if(event.m_event)
		clEnqueueWaitForEvents(g_context->m_queue, 1, (const cl_event[]){event.m_event});
	else 
		std::cerr << "waiting for null event!" << std::endl;
}

ComputeEvent compute_EnqueueMarker()
{
	cl_event ev = {};
	cl_int ret = clEnqueueMarker(g_context->m_queue, &ev);
	compute_CheckError(ret, "clEnqueueMarker");
	return ev;
}

void compute_Finish()
{
	if(!g_context) return ;
	clFinish(g_context->m_queue);
}

ComputeEvent compute_PrefixSum(unsigned int* in, unsigned int* out, unsigned int size)
{
	auto sumKernel = g_prefixSumProgram->CreateKernel("prefixSum");
	
	constexpr unsigned int blockSize = 512;
	const unsigned int numBlocks = (size + 511) / blockSize;
	unsigned int bufSize = numBlocks > 1 ? blockSize : size;
	unsigned int alignedBufSize = (bufSize + 3) & ~3;

	auto outBuffer = compute_CreateBufferWO(sizeof(unsigned int) * alignedBufSize);
	auto inBuffer = compute_CreateBufferRO(sizeof(unsigned int) * alignedBufSize);
	sumKernel->SetArg(0, outBuffer.get());
	sumKernel->SetArg(1, inBuffer.get());
	sumKernel->SetArgTempSize(3, 2 * blockSize * sizeof(unsigned int));

	ComputeEvent lastEv;
	unsigned int inOffset = 0;
	for(unsigned int i = 0; i < numBlocks; ++i)
	{
		unsigned int totalSizeLeft = size - inOffset;
		unsigned int curSize = Min(blockSize, totalSizeLeft);
		auto writeEv = inBuffer->EnqueueWrite(0, sizeof(unsigned int) * curSize, &in[inOffset], 
			lastEv.m_event ? 1 : 0, &lastEv.m_event);
		unsigned int curSizeArg = (curSize + 3) & ~3;
		sumKernel->SetArg(2, &curSizeArg);
		auto kernelEv = sumKernel->EnqueueEv(1, (const size_t[]){curSizeArg}, nullptr, 1, &writeEv.m_event);
		auto readEv = outBuffer->EnqueueRead(0, sizeof(unsigned int) * curSize, &out[inOffset],
			1, &kernelEv.m_event);
		lastEv = readEv;
		inOffset += blockSize;
	}

	// merge blocks now that each sub part has been summed
	if(numBlocks > 1)
	{
		auto mergeKernel = g_prefixSumProgram->CreateKernel("mergeSumBlock");
		mergeKernel->SetArg(0, outBuffer.get());
		mergeKernel->SetArg(1, inBuffer.get());


		inOffset = blockSize;
		for(unsigned int i = 1; i < numBlocks; ++i)
		{
			unsigned int totalSizeLeft = size - inOffset;
			unsigned int curSize = Min(blockSize, totalSizeLeft);
			compute_WaitForEvent(lastEv);
			unsigned int prevMax = out[ (i-1)*blockSize + (blockSize-1) ];
			mergeKernel->SetArg(2, &prevMax);
			std::cout << "prev max index = " <<  (i-1)*blockSize + (blockSize-1) <<" prev max = " << prevMax << std::endl;
			auto writeEv = inBuffer->EnqueueWrite(0, sizeof(unsigned int) * curSize, &out[inOffset],
				lastEv.m_event ? 1 : 0, &lastEv.m_event);
			auto kernelEv = mergeKernel->EnqueueEv(1, (const size_t[]){curSize}, nullptr, 1, &writeEv.m_event);
			auto readEv = outBuffer->EnqueueRead(0, sizeof(unsigned int) * curSize, &out[inOffset],
				1, &kernelEv.m_event);
			lastEv = readEv;
			inOffset += blockSize;
		}
	}
	
	return lastEv;
}


#pragma once

#include <GL/glew.h>
#include <CL/opencl.h>
#include <vector>
#include <string>

class SubmenuMenuItem;
class ComputeKernel;

////////////////////////////////////////////////////////////////////////////////
class ComputeProgram
{
public:
	ComputeProgram(cl_context ctx, const char* filename);
	~ComputeProgram();

	void Recompile();

	std::shared_ptr<ComputeKernel> CreateKernel(const char* kernelName);
private:
	std::string m_filename;
	cl_program m_program;
	cl_context m_context;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeKernel
{
public:
	ComputeKernel(cl_program prg, const char* kernelName);
	~ComputeKernel();
private:
	cl_kernel m_kernel;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeBuffer
{
public:
	ComputeBuffer(cl_context ctx, cl_mem_flags flags, size_t size, void* ptr);
	~ComputeBuffer();
private:
	cl_mem m_mem;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeImage
{
public:
	//ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format,
	//	const cl_image_desc *image_desc, void* ptr);
	ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format, 
			size_t width, size_t height, 
			size_t pitch, void* ptr);
	ComputeImage(cl_context ctx, cl_mem_flags flags, GLenum target, GLuint texture);
	~ComputeImage();
private:
	cl_mem m_mem;
};

////////////////////////////////////////////////////////////////////////////////
// information classes
class ComputeDevice
{
public:
	ComputeDevice(cl_device_id id);
	std::string GetTypeStr() const;

	cl_device_id m_id;
	std::string m_name;
	cl_device_type m_type;
	std::string m_vendor;
};

class ComputePlatform
{
public:
	ComputePlatform(cl_platform_id id);
	std::vector<ComputeDevice> GetDevices(cl_device_type type = CL_DEVICE_TYPE_ALL) const;

	cl_platform_id m_id;
	std::string m_name;
	std::string m_version;
	std::string m_profile;
	std::string m_vendor;
};

////////////////////////////////////////////////////////////////////////////////
// functions
void compute_Init(const char* requestedDevice);
void compute_CheckError(cl_int ret, const char* name);
std::string compute_GetCurrentDeviceName();
cl_device_id compute_FindDeviceByName(const char* name);
std::shared_ptr<SubmenuMenuItem> compute_CreateDeviceMenu();
std::vector<ComputePlatform> compute_GetPlatforms();
std::shared_ptr<ComputeProgram> compute_CompileProgram(const char* filename);

// buffer creation functions
std::shared_ptr<ComputeBuffer> compute_CreateReadOnlyBuffer(size_t size, void* hostData);
std::shared_ptr<ComputeBuffer> compute_CreateWriteOnlyBuffer(size_t size);

std::shared_ptr<ComputeImage> compute_CreateWriteOnlyImage2D(size_t width, size_t height, 
	cl_channel_order ord, cl_channel_type type);
std::shared_ptr<ComputeImage> compute_CreateWriteOnlyImageFromGL(GLenum target, GLuint tex);


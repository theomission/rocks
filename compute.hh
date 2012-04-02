#pragma once

#include <GL/glew.h>
#include <CL/opencl.h>
#include <vector>
#include <string>

class SubmenuMenuItem;
class ComputeKernel;
class ComputeBuffer;
class ComputeImage;

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
class ComputeEvent
{
public:
	ComputeEvent() : m_event(0) {}
	ComputeEvent(cl_event event) : m_event(event) {}
	ComputeEvent(const ComputeEvent& o) : m_event(o.m_event) { if(m_event) clRetainEvent(m_event); }
	ComputeEvent(ComputeEvent&& o) : m_event(o.m_event) { o.m_event = 0; }
	ComputeEvent& operator=(const ComputeEvent& o) { 
		if(this != &o)  {
			if(m_event) clReleaseEvent(m_event);
			m_event = o.m_event;
			if(m_event) clRetainEvent(m_event);
		}
		return *this;
	}
	~ComputeEvent() { if(m_event) clReleaseEvent(m_event); }

	cl_event m_event;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeKernel
{
public:
	ComputeKernel(cl_program prg, const char* kernelName);
	~ComputeKernel();

	void SetArg(int index, const ComputeBuffer* buffer) const;
	void SetArg(int index, const ComputeImage* image) const;
	void SetArg(int index, size_t arg_size, const void* value) const;
	template<class T>
		void SetArg(int index, const T* val) const {
			SetArg(index, sizeof(T), val);
		}
	template<class T>
		void SetArgVal(int index, const T& val) const {
			SetArg(index, sizeof(T), &val);
		}
	void SetArgTempSize(int index, size_t size) const;

	void Enqueue(cl_uint dims, const size_t* globalWorkSize, const size_t* localWorkSize = nullptr,
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;
	ComputeEvent EnqueueEv(cl_uint dims, const size_t* globalWorkSize, const size_t* localWorkSize = nullptr,
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;

	void Enqueue(cl_uint dims, const size_t* globalWorkOffset,
		const size_t* globalWorkSize, const size_t* localWorkSize,
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;
	ComputeEvent EnqueueEv(cl_uint dims, const size_t* globalWorkOffset,
		const size_t* globalWorkSize, const size_t* localWorkSize,
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;
private:
	cl_kernel m_kernel;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeBuffer
{
	friend class ComputeKernel;
public:
	ComputeBuffer(cl_context ctx, cl_mem_flags flags, size_t size, void* ptr);
	~ComputeBuffer();

	ComputeEvent EnqueueRead(size_t offset, size_t cb, void* hostMem, 
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;
	ComputeEvent EnqueueWrite(size_t offset, size_t cb, const void* hostMem, 
		cl_uint numEvents = 0, const cl_event* events = nullptr) const;
private:
	cl_mem m_mem;
};

////////////////////////////////////////////////////////////////////////////////
class ComputeImage
{
	friend class ComputeKernel;
public:
	//ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format,
	//	const cl_image_desc *image_desc, void* ptr);
	ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format, 
			size_t width, size_t height, 
			size_t pitch, void* ptr);
	ComputeImage(cl_context ctx, cl_mem_flags flags, const cl_image_format* image_format, 
			size_t width, size_t height, size_t depth,
			size_t rowPitch, size_t slicePitch, void* ptr);
	ComputeImage(cl_context ctx, cl_mem_flags flags, GLenum target, GLuint texture);
	~ComputeImage();

	ComputeEvent EnqueueRead(const size_t origin[3], const size_t region[3], void* ptr);
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
std::shared_ptr<ComputeBuffer> compute_CreateBufferRO(size_t size, const void* hostData = nullptr);
std::shared_ptr<ComputeBuffer> compute_CreateBufferRW(size_t size, const void* hostData = nullptr);
std::shared_ptr<ComputeBuffer> compute_CreateBufferWO(size_t size);

std::shared_ptr<ComputeImage> compute_CreateImage2DWO(size_t width, size_t height, 
	cl_channel_order ord, cl_channel_type type);
std::shared_ptr<ComputeImage> compute_CreateImage3DWO(size_t width, size_t height, size_t depth,
	cl_channel_order ord, cl_channel_type type);
std::shared_ptr<ComputeImage> compute_CreateImageFromGLWO(GLenum target, GLuint tex);
void compute_EnqueueWaitForEvent(const ComputeEvent& event);
void compute_WaitForEvent(const ComputeEvent& event);
ComputeEvent compute_EnqueueMarker();
void compute_Finish();
ComputeEvent compute_PrefixSum(unsigned int* in, unsigned int* out, unsigned int size);



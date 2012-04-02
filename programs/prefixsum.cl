
inline uint scan1Inclusive(uint data, __local unsigned int *temp, unsigned int size)
{
	uint index = get_local_id(0);
	temp[index] = 0;
	index += size;
	temp[index] = data;

	for(uint offset = 1; offset < size; offset <<= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);
		uint tempData = temp[index] + temp[index - offset];
		barrier(CLK_LOCAL_MEM_FENCE);
		temp[index] = tempData;
	}

	return temp[index];
}

uint4 scan4Inclusive(uint4 data, __local unsigned int *temp, unsigned int size)
{
	data.y += data.x;
	data.z += data.y;
	data.w += data.z;
	uint val = scan1Inclusive(data.w, temp, size) - data.w;
	return data + (uint4)val;
}

__kernel void prefixSum(
	__global uint4 *outData,
	__global uint4 *inData,
	unsigned int num,		
	__local unsigned int *temp)
{
	int index = get_global_id(0);

	uint4 idata = inData[index];
	uint4 odata = scan4Inclusive(idata, temp, num / 4);
	outData[index] = odata;
}

__kernel void mergeSumBlock(
	__global uint4 *outData,
	__global uint4 *inData,
	unsigned int val)
{
	int index = get_global_id(0);
	uint4 idata = inData[index];
	uint4 odata = idata + (int4)(val);
	outData[index] = odata;
}


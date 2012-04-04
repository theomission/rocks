#pragma once

#include <memory>
class TriSoup;

std::shared_ptr<TriSoup> surfcon_CreateMeshFromDensityField(
	float isolevel,
	const float* densityField, 
	unsigned int width, unsigned int height, unsigned int depth);


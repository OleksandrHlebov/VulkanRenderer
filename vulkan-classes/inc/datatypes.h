#ifndef DATATYPES_H
#define DATATYPES_H
#include "glm/glm.hpp"

struct ModelViewProj
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

#endif //DATATYPES_H

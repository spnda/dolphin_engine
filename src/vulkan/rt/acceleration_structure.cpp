#include "acceleration_structure.hpp"

#include "../context.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, const AccelerationStructureType type, std::string  asName)
        : ctx(context), name(std::move(asName)), type(type),
          resultBuffer(context, name + " resultBuffer") {
}

dp::AccelerationStructure::AccelerationStructure(const dp::AccelerationStructure& structure)
        : ctx(structure.ctx), name(structure.name), resultBuffer(structure.resultBuffer),
          handle(structure.handle), address(structure.address), type(structure.type) {
}

dp::AccelerationStructure& dp::AccelerationStructure::operator=(const dp::AccelerationStructure& structure) {
    this->address = structure.address;
    this->handle = structure.handle;
    this->name = structure.name;
    this->type = structure.type;
    this->resultBuffer = structure.resultBuffer;
    return *this;
}

void dp::AccelerationStructure::setName() {
    ctx.setDebugUtilsName(handle, name);
}

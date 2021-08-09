#include "bottom_level_geometry.hpp"

uint32_t dp::BottomLevelGeometry::size() const {
    return static_cast<uint32_t>(structureGeometries.size());
}

const std::vector<VkAccelerationStructureGeometryKHR>& dp::BottomLevelGeometry::getGeometry() const {
    return structureGeometries;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& dp::BottomLevelGeometry::getBuildOffsetInfos() const {
    return buildOffsetInfos;
}

// TODO: Make this function only accept the buffers and offsets.
void dp::BottomLevelGeometry::addGeometryTriangles(dp::Buffer& vertexBuffer, uint32_t vertexCount, dp::Buffer& indexBuffer, uint32_t indexCount, bool isOpaque) {
    uint32_t vertexSizeof = sizeof(float) * 3;
    
    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.pNext = nullptr;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.pNext = nullptr;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexBuffer.address;
    geometry.geometry.triangles.vertexStride = vertexSizeof; // TODO: Size of a vertex
    geometry.geometry.triangles.maxVertex = vertexCount;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.indexData.deviceAddress = indexBuffer.address;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.transformData = {};
    geometry.flags = isOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

    VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo = {};
    buildOffsetInfo.firstVertex = 0;
    buildOffsetInfo.primitiveOffset = 0;
    buildOffsetInfo.primitiveCount = indexCount / 3;
    buildOffsetInfo.transformOffset = 0;

    structureGeometries.push_back(geometry);
    buildOffsetInfos.emplace_back(buildOffsetInfo);
}

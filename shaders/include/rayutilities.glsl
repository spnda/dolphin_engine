/** We assume this file is included *after* the instance description and geometry description
 * buffers have been declared. */
Triangle getTriangle(in uint index, in uint geometryIndex, in uint primitive, in const vec3 barycentrics) {
    Triangle tri;

    InstanceDescription instance = instances.d[index];

    GeometryDescriptions geometries = GeometryDescriptions(instance.geometryBufferAddress);
    GeometryDescription geometry = geometries.d[geometryIndex];
    tri.materialIndex = geometry.materialIndex;

    Indices indices = Indices(instance.indexBufferAddress + geometry.indexOffset);
    Vertices vertices = Vertices(instance.vertexBufferAddress + geometry.vertexOffset);

    ivec3 primIndices = indices.i[primitive];
    tri.vert[0] = vertices.v[primIndices.x];
    tri.vert[1] = vertices.v[primIndices.y];
    tri.vert[2] = vertices.v[primIndices.z];

    return tri;
}

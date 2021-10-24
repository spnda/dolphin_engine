Triangle getTriangle(in uint index, in uint primitive, in const vec3 barycentrics) {
    Triangle tri;

    ObjectDescription description = descriptions.d[gl_InstanceCustomIndexEXT];
    tri.materialIndex = description.materialIndex;

    Indices indices = Indices(description.indexBufferAddress);
    Vertices vertices = Vertices(description.vertexBufferAddress);
    ivec3 primIndices = indices.i[primitive];
    tri.vert[0] = vertices.v[primIndices.x];
    tri.vert[1] = vertices.v[primIndices.y];
    tri.vert[2] = vertices.v[primIndices.z];

    return tri;
}

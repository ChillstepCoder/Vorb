#pragma once

// Should be included in RenderThreadTasks

class MeshTaskData {
public:
    // TODO: Are we guarenteeing quads?
    MeshTaskData(
        ContainerMeshBuilders&& builders
    ) : 
        builders(std::move(builders))
    {};

    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);

    ContainerMeshBuilders builders;
};

// TODO: We should make sure we dont build this on dedicated server as it initializes some memory
struct mesh_task_pool {};
using mesh_singleton_task_pool = boost::singleton_pool<mesh_task_pool, sizeof(MeshTaskData), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 256u>;

void* MeshTaskData::operator new(size_t count) {
    UNUSED(count);
    return mesh_singleton_task_pool::malloc();
}

void MeshTaskData::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return mesh_singleton_task_pool::free(pointer);
}
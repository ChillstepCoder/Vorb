#pragma once

class CharacterModelTaskData {
public:
    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);

    entt::entity entityId;
    ui32 modelId;
};
static_assert(sizeof(CharacterModelTaskData) == 8);

// TODO: We should make sure we dont build this on dedicated server as it initializes some memory
struct character_task_pool {};
using character_singleton_task_pool = boost::singleton_pool<character_task_pool, sizeof(CharacterModelTaskData), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 128u>;

void* CharacterModelTaskData::operator new(size_t count) {
    UNUSED(count);
    return character_singleton_task_pool::malloc();
}

void CharacterModelTaskData::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return character_singleton_task_pool::free(pointer);
}

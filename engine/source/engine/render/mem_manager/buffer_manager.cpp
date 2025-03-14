#include "pch.h"
#include "buffer_manager.h"

#include "utils/debug/assertion.h"

#include "render/platform/OpenGL/opengl_driver.h"


static std::unique_ptr<MemoryBufferManager> pMemoryBufferMngInst = nullptr;

static constexpr size_t MAX_MEM_BUFFER_COUNT = 4096;


static GLbitfield GetMemoryBufferCreationFlagsGL(MemoryBufferCreationFlags flags) noexcept
{
    GLbitfield result = BUFFER_CREATION_FLAG_ZERO;

    result |= (flags & BUFFER_CREATION_FLAG_DYNAMIC_STORAGE ? GL_DYNAMIC_STORAGE_BIT : 0);
    result |= (flags & BUFFER_CREATION_FLAG_READABLE ? GL_MAP_READ_BIT : 0);
    result |= (flags & BUFFER_CREATION_FLAG_WRITABLE ? GL_MAP_WRITE_BIT : 0);
    result |= (flags & BUFFER_CREATION_FLAG_PERSISTENT ? GL_MAP_PERSISTENT_BIT : 0);
    result |= (flags & BUFFER_CREATION_FLAG_COHERENT ? GL_MAP_COHERENT_BIT : 0);
    result |= (flags & BUFFER_CREATION_FLAG_CLIENT_STORAGE ? GL_CLIENT_STORAGE_BIT : 0);
    
    return result;
}


static GLenum TranslateMemoryBufferTypeToGL(MemoryBufferType type) noexcept
{
    switch(type) {
        case MemoryBufferType::TYPE_VERTEX_BUFFER:           return GL_ARRAY_BUFFER;
        case MemoryBufferType::TYPE_INDEX_BUFFER:            return GL_ELEMENT_ARRAY_BUFFER;
        case MemoryBufferType::TYPE_CONSTANT_BUFFER:         return GL_UNIFORM_BUFFER;
        case MemoryBufferType::TYPE_UNORDERED_ACCESS_BUFFER: return GL_SHADER_STORAGE_BUFFER;
        default:
            ENG_ASSERT_FAIL("Invalid memory buffer type");
            return GL_NONE;
    }
}


static bool IsBufferIndexedBindable(MemoryBufferType type) noexcept
{
    switch(type) {
        case MemoryBufferType::TYPE_VERTEX_BUFFER:           return false;
        case MemoryBufferType::TYPE_INDEX_BUFFER:            return false;
        case MemoryBufferType::TYPE_CONSTANT_BUFFER:         return true;
        case MemoryBufferType::TYPE_UNORDERED_ACCESS_BUFFER: return true;
        default:
            ENG_ASSERT_FAIL("Invalid memory buffer type");
            return GL_NONE;
    }
}


MemoryBuffer::MemoryBuffer(MemoryBuffer &&other) noexcept
{
#if defined(ENG_DEBUG)
    std::swap(m_dbgName, other.m_dbgName);
#endif
    std::swap(m_ID, other.m_ID);
    std::swap(m_size, other.m_size);
    std::swap(m_renderID, other.m_renderID);
    std::swap(m_elementSize, other.m_elementSize);
    std::swap(m_type, other.m_type);
    std::swap(m_creationFlags, other.m_creationFlags);
}


MemoryBuffer &MemoryBuffer::operator=(MemoryBuffer&& other) noexcept
{
    Destroy();

#if defined(ENG_DEBUG)
    std::swap(m_dbgName, other.m_dbgName);
#endif
    std::swap(m_ID, other.m_ID);
    std::swap(m_size, other.m_size);
    std::swap(m_renderID, other.m_renderID);
    std::swap(m_elementSize, other.m_elementSize);
    std::swap(m_type, other.m_type);
    std::swap(m_creationFlags, other.m_creationFlags);

    return *this;
}


void MemoryBuffer::FillSubdata(size_t offset, size_t size, const void *pData) noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    ENG_ASSERT(IsDynamicStorage(), "Memory buffer \'{}\' was not created with BUFFER_CREATION_FLAG_DYNAMIC_STORAGE flag", m_dbgName.CStr());
    glNamedBufferSubData(m_renderID, offset, size, pData);
}


void MemoryBuffer::Clear(size_t offset, size_t size, const void *pData) noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    glClearNamedBufferSubData(m_renderID, GL_R32F, offset, size, GL_RED, GL_FLOAT, pData);
}


void MemoryBuffer::Clear() noexcept
{
    Clear(0, m_size, nullptr);
}


void MemoryBuffer::Bind() noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());

    const GLenum target = TranslateMemoryBufferTypeToGL(m_type);
    glBindBuffer(target, m_renderID);
}


void MemoryBuffer::BindIndexed(uint32_t index) noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    ENG_ASSERT(IsBufferIndexedBindable(m_type), "Memory buffer \'{}\' is not indexed bindable", m_dbgName.CStr());

    const GLenum target = TranslateMemoryBufferTypeToGL(m_type);
    glBindBufferBase(target, index, m_renderID);
}


const void* MemoryBuffer::MapRead() noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    ENG_ASSERT(IsReadable(), "Memory buffer \'{}\' was not created with BUFFER_CREATION_FLAG_READABLE flag", m_dbgName.CStr());
    
    return glMapNamedBuffer(m_renderID, GL_READ_ONLY);
}


void* MemoryBuffer::MapWrite() noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    ENG_ASSERT(IsDynamicStorage(), "Memory buffer \'{}\' was not created with GL_DYNAMIC_STORAGE_BIT flag", m_dbgName.CStr());
    ENG_ASSERT(IsWritable(), "Memory buffer \'{}\' was not created with BUFFER_CREATION_FLAG_WRITABLE flag", m_dbgName.CStr());
    
    return glMapNamedBuffer(m_renderID, GL_WRITE_ONLY);
}


void* MemoryBuffer::MapReadWrite() noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    ENG_ASSERT(IsDynamicStorage(), "Memory buffer \'{}\' was not created with GL_DYNAMIC_STORAGE_BIT flag", m_dbgName.CStr());
    ENG_ASSERT(IsWritable(), "Memory buffer \'{}\' was not created with BUFFER_CREATION_FLAG_WRITABLE flag", m_dbgName.CStr());
    ENG_ASSERT(IsReadable(), "Memory buffer \'{}\' was not created with BUFFER_CREATION_FLAG_READABLE flag", m_dbgName.CStr());

    return glMapNamedBuffer(m_renderID, GL_READ_WRITE);
}


bool MemoryBuffer::Unmap() const noexcept
{
    ENG_ASSERT(IsValid(), "Memory buffer \'{}\' is invalid", m_dbgName.CStr());
    
    return glUnmapNamedBuffer(m_renderID);
}


bool MemoryBuffer::IsValid() const noexcept
{
    return m_ID.IsValid() && m_type != MemoryBufferType::TYPE_INVALID && m_renderID != 0;
}


void MemoryBuffer::SetDebugName(ds::StrID name) noexcept
{
#if defined(ENG_DEBUG)
    m_dbgName = name;
#endif
}


ds::StrID MemoryBuffer::GetDebugName() const noexcept
{
#if defined(ENG_DEBUG)
    return m_dbgName;
#else
    return "";
#endif
}


uint64_t MemoryBuffer::GetElementCount() const noexcept
{
    ENG_ASSERT(m_elementSize != 0, "Memory buffer \'{}\' element size is 0", m_dbgName.CStr());
    ENG_ASSERT(m_size % m_elementSize == 0, "Buffer \'{}\' size must be multiple of element size", m_dbgName.CStr());
    
    return m_size / m_elementSize;
}


bool MemoryBuffer::Create(const MemoryBufferCreateInfo& createInfo) noexcept
{
    ENG_ASSERT(!IsValid(), "Attempt to create already valid memory buffer: {}", m_dbgName.CStr());

    ENG_ASSERT(m_ID.IsValid(), "Buffer ID is invalid. You must initialize only buffers which were returned by MemoryBufferManager");
    ENG_ASSERT(createInfo.type != MemoryBufferType::TYPE_INVALID && createInfo.type < MemoryBufferType::TYPE_COUNT,
        "Invalid \'{}\' buffer create info type", m_dbgName.CStr());

    ENG_ASSERT(createInfo.dataSize > 0, "Invalid \'{}\' buffer create info data size", m_dbgName.CStr());
    ENG_ASSERT(createInfo.elementSize > 0, "Invalid \'{}\' buffer create info data element size", m_dbgName.CStr());
    ENG_ASSERT(createInfo.dataSize % createInfo.elementSize == 0, "Data size is must be multiple of element size");

    glCreateBuffers(1, &m_renderID);

    const GLbitfield creationFlags = GetMemoryBufferCreationFlagsGL(createInfo.creationFlags);
    glNamedBufferStorage(m_renderID, createInfo.dataSize, createInfo.pData, creationFlags);

    m_size = createInfo.dataSize;
    m_elementSize = createInfo.elementSize;
    m_type = createInfo.type;
    m_creationFlags = createInfo.creationFlags;

    return true;
}


void MemoryBuffer::Destroy() noexcept
{
    if (!IsValid()) {
        return;
    }

    glDeleteBuffers(1, &m_renderID);

#if defined(ENG_DEBUG)
    m_dbgName = "";
#endif

    m_size = 0;
    m_elementSize = 0;
    m_type = MemoryBufferType::TYPE_INVALID;
    m_creationFlags = MemoryBufferCreationFlags::BUFFER_CREATION_FLAG_ZERO;
    m_renderID = 0;
}


MemoryBufferManager& MemoryBufferManager::GetInstance() noexcept
{
    ENG_ASSERT(engIsMemoryBufferManagerInitialized(), "Memory buffer manager is not initialized");
    return *pMemoryBufferMngInst;
}


MemoryBufferManager::~MemoryBufferManager()
{
    Terminate();
}


MemoryBuffer* MemoryBufferManager::RegisterBuffer() noexcept
{
    const BufferID bufferID = m_IDPool.Allocate();
    ENG_ASSERT(bufferID.Value() < m_buffersStorage.size(), "Memory buffer storage overflow");
    
    MemoryBuffer* pBuffer = &m_buffersStorage[bufferID.Value()];

    ENG_ASSERT(!pBuffer->IsValid(), "Valid buffer was returned during registration");

    pBuffer->m_ID = bufferID;

    return pBuffer;
}


void MemoryBufferManager::UnregisterBuffer(MemoryBuffer* pBuffer)
{
    if (!pBuffer) {
        return;
    }

    if (pBuffer->IsValid()) {
        ENG_LOG_WARN("Unregistration of buffer \'{}\' while it's steel valid. Prefer to destroy buffers manually", pBuffer->GetDebugName().CStr());
        pBuffer->Destroy();
    }

    m_IDPool.Deallocate(pBuffer->m_ID);
}


bool MemoryBufferManager::Init() noexcept
{
    if (IsInitialized()) {
        return true;
    }

    m_buffersStorage.resize(MAX_MEM_BUFFER_COUNT);
    m_IDPool.Reset();
    m_isInitialized = true;

    return true;
}


void MemoryBufferManager::Terminate() noexcept
{
    m_buffersStorage.clear();
    m_IDPool.Reset();
    m_isInitialized = false;
}


bool MemoryBufferManager::IsInitialized() const noexcept
{
    return m_isInitialized;
}


bool engInitMemoryBufferManager() noexcept
{
    if (engIsMemoryBufferManagerInitialized()) {
        ENG_LOG_WARN("Memory buffer manager is already initialized!");
        return true;
    }

    pMemoryBufferMngInst = std::unique_ptr<MemoryBufferManager>(new MemoryBufferManager);

    if (!pMemoryBufferMngInst) {
        ENG_ASSERT_FAIL("Failed to allocate memory for memory buffer manager");
        return false;
    }

    if (!pMemoryBufferMngInst->Init()) {
        ENG_ASSERT_FAIL("Failed to initialized memory buffer manager");
        return false;
    }

    return true;
}


void engTerminateMemoryBufferManager() noexcept
{
    pMemoryBufferMngInst = nullptr;
}


bool engIsMemoryBufferManagerInitialized() noexcept
{
    return pMemoryBufferMngInst && pMemoryBufferMngInst->IsInitialized();
}
#pragma once

#include "utils/file/file.h"
#include "utils/data_structures/strid.h"
#include "utils/data_structures/base_id.h"

#include "resource_bind.h"

#include "core.h"

#include <deque>

#include <limits>
#include <cstdint>


enum class ShaderStageType : uint32_t
{
    VERTEX,
    PIXEL,
    COUNT
};


struct ShaderStageCreateInfo
{
    const char* pSourceCode;
    const char** pDefines;
    const char* pIncludeParentPath;
    uint32_t codeSize;
    uint32_t definesCount;
    ShaderStageType type;
};


struct ShaderProgramCreateInfo
{
    const ShaderStageCreateInfo** pStageCreateInfos;
    size_t stageCreateInfosCount;
};


using ProgramID = ds::BaseID<uint32_t>;


class ShaderProgram
{
    friend class ShaderManager;

public:
    ShaderProgram(const ShaderProgram& other) = delete;
    ShaderProgram& operator=(const ShaderProgram& other) = delete;

    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    ShaderProgram() = default;
    ~ShaderProgram() { Destroy(); }

    bool Create(const ShaderProgramCreateInfo& createInfo) noexcept;
    void Destroy() noexcept;

    void Bind() const noexcept;

    ENG_DEPRECATED("Prefer to use uniform buffers")
    void SetLocalSrvBool(const ShaderResourceBindStruct<ShaderResourceType::TYPE_BOOL>& bind, bool value) noexcept;
    ENG_DEPRECATED("Prefer to use uniform buffers")
    void SetLocalSrvInt(const ShaderResourceBindStruct<ShaderResourceType::TYPE_INT>& bind, int32_t value) noexcept;
    ENG_DEPRECATED("Prefer to use uniform buffers")
    void SetLocalSrvUInt(const ShaderResourceBindStruct<ShaderResourceType::TYPE_UINT>& bind, uint32_t value) noexcept;
    ENG_DEPRECATED("Prefer to use uniform buffers")
    void SetLocalSrvFloat(const ShaderResourceBindStruct<ShaderResourceType::TYPE_FLOAT>& bind, float value) noexcept;
    ENG_DEPRECATED("Prefer to use uniform buffers")
    void SetLocalSrvDouble(const ShaderResourceBindStruct<ShaderResourceType::TYPE_DOUBLE>& bind, double value) noexcept;

    bool IsValid() const noexcept;

    void SetDebugName(ds::StrID name) noexcept;
    
    ds::StrID GetDebugName() const noexcept;
    uint32_t GetRenderID() const noexcept { return m_renderID; }
    
    uint64_t Hash() const noexcept;

private:
    bool GetLinkingStatus() const noexcept;

private:
#if defined(ENG_DEBUG)
    ds::StrID m_dbgName = "_INVALID_";
#endif

    uint32_t m_renderID = 0;
    ProgramID m_ID;
};


class ShaderManager
{
    friend bool engInitShaderManager() noexcept;
    friend void engTerminateShaderManager() noexcept;
    friend bool engIsShaderManagerInitialized() noexcept;

public:
    static ShaderManager& GetInstance() noexcept;
    
public:
    ShaderManager(const ShaderManager& other) = delete;
    ShaderManager& operator=(const ShaderManager& other) = delete;
    ShaderManager(ShaderManager&& other) noexcept = delete;
    ShaderManager& operator=(ShaderManager&& other) noexcept = delete;

    ~ShaderManager();

    ShaderProgram* RegisterShaderProgram() noexcept;
    void UnregisterShaderProgram(ShaderProgram* pProgram) noexcept;
    
private:
    ShaderManager() = default;

    bool Init() noexcept;
    void Terminate() noexcept;

    bool IsInitialized() const noexcept;

private:
    std::vector<ShaderProgram> m_shaderProgramsStorage;
    
    using ProgramIDPool = ds::BaseIDPool<ProgramID>;
    ProgramIDPool m_IDPool;

    bool m_isInitialized = false;
};

uint64_t amHash(const ShaderProgram& program) noexcept;


bool engInitShaderManager() noexcept;
void engTerminateShaderManager() noexcept;
bool engIsShaderManagerInitialized() noexcept;
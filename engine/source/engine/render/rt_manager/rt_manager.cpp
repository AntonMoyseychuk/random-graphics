#include "pch.h"
#include "rt_manager.h"

#include "engine/event_system/event_dispatcher.h"
#include "engine/engine.h"

#include "engine/render/platform/OpenGL/opengl_driver.h"

#include "utils/data_structures/hash.h"

#include "engine/auto/auto_texture_incl.h"
#include "engine/auto/auto_registers_common.h"


static std::unique_ptr<RenderTargetManager> s_pRenderTargetMng = nullptr;

static int32_t MAX_COLOR_ATTACHMENTS = 0;


FrameBuffer::~FrameBuffer()
{
    Destroy();
}


FrameBuffer::FrameBuffer(FrameBuffer &&other) noexcept
{
#if defined(ENG_DEBUG)
    std::swap(m_attachments, other.m_attachments);
    std::swap(m_dbgName, other.m_dbgName);
#endif

    std::swap(m_attachmentsState, other.m_attachmentsState);
    std::swap(m_ID, other.m_ID);
    std::swap(m_renderID, other.m_renderID);
}


FrameBuffer &FrameBuffer::operator=(FrameBuffer &&other) noexcept
{
    Destroy();

#if defined(ENG_DEBUG)
    std::swap(m_attachments, other.m_attachments);
    std::swap(m_dbgName, other.m_dbgName);
#endif

    std::swap(m_attachmentsState, other.m_attachmentsState);
    std::swap(m_ID, other.m_ID);
    std::swap(m_renderID, other.m_renderID);

    return *this;
}


void FrameBuffer::Bind() noexcept
{
    ENG_ASSERT_GRAPHICS_API(IsValid(), "Attempt to bind invalid framebuffer");
    glBindFramebuffer(GL_FRAMEBUFFER, m_renderID);
}


bool FrameBuffer::IsValid() const noexcept
{
    return m_renderID != 0 && m_ID < RTFrameBufferID::RT_FRAMEBUFFER_COUNT && m_ID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID;
}


ds::StrID FrameBuffer::GetName() const noexcept
{
#if defined(ENG_DEBUG)
    return m_dbgName;
#else
    return "";
#endif
}


bool FrameBuffer::Init(ds::StrID dbgName, const FramebufferCreateInfo &createInfo) noexcept
{
#if defined(ENG_DEBUG)
    if (IsValid()) {
        if (m_dbgName != dbgName) {
            ENG_LOG_GRAPHICS_API_WARN("Reinitializing of \'{}\' framebuffer to \'{}\'", m_dbgName.CStr(), dbgName.CStr());
        } else {
            ENG_LOG_GRAPHICS_API_WARN("Reinitializing of \'{}\' framebuffer", m_dbgName.CStr());
        }
    }
#endif

    return Recreate(dbgName, createInfo);
}


void FrameBuffer::Destroy() noexcept
{
    glDeleteFramebuffers(1, &m_renderID);

#if defined(ENG_DEBUG)
    m_attachments.clear();
    m_dbgName = "";
#endif

    m_attachmentsState.colorAttachmentsCount = 0;
    m_attachmentsState.depthAttachmentsCount = 0;
    m_attachmentsState.stencilAttachmentsCount = 0;

    m_ID = RTFrameBufferID::RT_FRAMEBUFFER_INVALID;
    m_renderID = 0;
}


bool FrameBuffer::Recreate(ds::StrID dbgName, const FramebufferCreateInfo& createInfo) noexcept
{
    ENG_ASSERT_GRAPHICS_API(engIsTextureManagerInitialized(), "Texture manager must be initialized before framebuffers initializing");
    
    ENG_ASSERT_GRAPHICS_API(createInfo.ID < RTFrameBufferID::RT_FRAMEBUFFER_COUNT && createInfo.ID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, 
        "Framebufer ID is invalid");
    ENG_ASSERT_GRAPHICS_API(createInfo.attachmentsCount > 0 && createInfo.attachmentsCount <= (uint32_t)MAX_COLOR_ATTACHMENTS, "Invalid attachments count");
    ENG_ASSERT_GRAPHICS_API(createInfo.pAttachments, "Attachements are nullptr (\'{}\')", dbgName.CStr());

    if (IsValid()) {
        Destroy();
    }

    glCreateFramebuffers(1, &m_renderID);

    uint32_t attachmentsWidth = 0;
    uint32_t attachmentsHeight = 0;

    bool isFirstAttachment = true;

#if defined(ENG_DEBUG)
    m_attachments.reserve(createInfo.attachmentsCount);
#endif

    static auto GetFrameBufferAttachmentGLType = [](const FrameBufferAttachment& attachment) -> GLenum
    {
        switch(attachment.type) {
            case FrameBufferAttachmentType::TYPE_COLOR_ATTACHMENT:
                ENG_ASSERT_GRAPHICS_API(attachment.index < (uint32_t)MAX_COLOR_ATTACHMENTS, "Invalid color attachment index");
                return GL_COLOR_ATTACHMENT0 + attachment.index;
            case FrameBufferAttachmentType::TYPE_DEPTH_ATTACHMENT:
                return GL_DEPTH_ATTACHMENT;
            case FrameBufferAttachmentType::TYPE_STENCIL_ATTACHMENT:
                return GL_STENCIL_ATTACHMENT;
            case FrameBufferAttachmentType::TYPE_DEPTH_STENCIL_ATTACHMENT:
                return GL_DEPTH_STENCIL_ATTACHMENT;
            default:
                ENG_ASSERT_GRAPHICS_API_FAIL("Invalid frame buffer attachment type");
                return GL_NONE;
        }
    };

    for (size_t attachmentIdx = 0; attachmentIdx < createInfo.attachmentsCount; ++attachmentIdx) {
        const FrameBufferAttachment& attachment = createInfo.pAttachments[attachmentIdx];
        Texture* pTex = attachment.pTexure;

        const uint32_t texWidth = pTex->GetWidth();
        const uint32_t texHeight = pTex->GetHeight();

        ENG_ASSERT_GRAPHICS_API(pTex, "Attachment {} of \'{}\' framebuffer is nullptr", attachmentIdx, dbgName.CStr());
        ENG_ASSERT_GRAPHICS_API(pTex->IsValid(), "Invalid color attachment");
        ENG_ASSERT_GRAPHICS_API(pTex->IsType2D(), "Invalid color attachment type. Only 2D textures are supported for now");

        if (isFirstAttachment) {
            attachmentsWidth = texWidth;
            attachmentsHeight = texHeight;
            isFirstAttachment = false;
        } else {
            ENG_ASSERT_GRAPHICS_API(attachmentsWidth == texWidth && attachmentsHeight == texHeight, "Attachments dimensions must be equal");
        }

    #if defined(ENG_DEBUG)
        m_attachments.emplace_back(attachment);
    #endif

        const GLenum attachmentGLType = GetFrameBufferAttachmentGLType(attachment);
        if (attachmentGLType == GL_NONE) {
            Destroy();
            return false;
        }

        if (attachment.type == FrameBufferAttachmentType::TYPE_COLOR_ATTACHMENT) {
            ++m_attachmentsState.colorAttachmentsCount;
        } else if (attachment.type == FrameBufferAttachmentType::TYPE_DEPTH_STENCIL_ATTACHMENT) {
            m_attachmentsState.depthAttachmentsCount = 1;
            m_attachmentsState.stencilAttachmentsCount = 1;
        } else if (attachment.type == FrameBufferAttachmentType::TYPE_DEPTH_ATTACHMENT) {
            m_attachmentsState.depthAttachmentsCount = 1;
        } else if (attachment.type == FrameBufferAttachmentType::TYPE_STENCIL_ATTACHMENT) {
            m_attachmentsState.stencilAttachmentsCount = 1;
        }

        glNamedFramebufferTexture(m_renderID, attachmentGLType, pTex->GetRenderID(), 0);
    }

    if (!CheckCompleteStatus()) {
        ENG_ASSERT_GRAPHICS_API_FAIL("FrameBuffer \'{}\' is incomplete");
        Destroy();
        return false;
    }

#if defined(ENG_DEBUG)
    m_dbgName = dbgName;
#endif
    m_ID = createInfo.ID;

    return true;
}


bool FrameBuffer::CheckCompleteStatus() const noexcept
{
    const GLenum frameBufferStatus = glCheckNamedFramebufferStatus(m_renderID, GL_FRAMEBUFFER);

    switch(frameBufferStatus) {
        case GL_FRAMEBUFFER_COMPLETE:
            return true;
        case GL_FRAMEBUFFER_UNDEFINED: 
            ENG_LOG_GRAPHICS_API_ERROR("Specified framebuffer is the default read or draw framebuffer, but the default framebuffer does not exist");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            ENG_LOG_GRAPHICS_API_ERROR("Any of the framebuffer attachment points are framebuffer incomplete");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            ENG_LOG_GRAPHICS_API_ERROR("Framebuffer does not have at least one image attached to it");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            ENG_LOG_GRAPHICS_API_ERROR("Value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for any color attachment point(s) named by GL_DRAW_BUFFERi");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            ENG_LOG_GRAPHICS_API_ERROR("GL_READ_BUFFER is not GL_NONE and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment point named by GL_READ_BUFFER");
            return false;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            ENG_LOG_GRAPHICS_API_ERROR("Combination of internal formats of the attached images violates an implementation-dependent set of restrictions");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            ENG_LOG_GRAPHICS_API_ERROR(
                "Value of GL_RENDERBUFFER_SAMPLES is not the same for all attached renderbuffers;"
                "if the value of GL_TEXTURE_SAMPLES is the not same for all attached textures;"
                "or, if the attached images are a mix of renderbuffers and textures,"
                "the value of GL_RENDERBUFFER_SAMPLES does not match the value of GL_TEXTURE_SAMPLES");
            ENG_LOG_GRAPHICS_API_ERROR("Or Value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not the same for all attached textures;"
                "or, if the attached images are a mix of renderbuffers and textures, "
                "the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS is not GL_TRUE for all attached textures.");
            return false;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            ENG_LOG_GRAPHICS_API_ERROR("Any framebuffer attachment is layered, and any populated attachment is not layered,"
            "or if all populated color attachments are not from textures of the same target.");
            return false;
            
        default:
            ENG_ASSERT_GRAPHICS_API_FAIL("Unknown frame buffer status");
            return false;
    }
}


RenderTargetManager &RenderTargetManager::GetInstance() noexcept
{
    ENG_ASSERT_GRAPHICS_API(engIsRenderTargetManagerInitialized(), "Render target manager is not initialized");
    return *s_pRenderTargetMng;
}


RenderTargetManager::~RenderTargetManager()
{
    Terminate();
}


Texture *RenderTargetManager::GetRTTexture(RTTextureID texID) noexcept
{
    ENG_ASSERT_GRAPHICS_API(texID != RTTextureID::RT_TEX_INVALID && texID < RTTextureID::RT_TEX_COUNT, "Invalid RT texture ID");

    Texture* pRTTexture = m_RTTextures[static_cast<size_t>(texID)];
    ENG_ASSERT_GRAPHICS_API(pRTTexture != nullptr, "RT texture is nullptr");
    
    return pRTTexture;
}


void RenderTargetManager::BindFrameBuffer(RTFrameBufferID framebufferID) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, 
        "Invalid RT framebuffer ID");
    m_frameBuffers[static_cast<size_t>(framebufferID)].Bind();
}


void RenderTargetManager::ClearFrameBuffer(RTFrameBufferID framebufferID, float r, float g, float b, float a, float depth, int32_t stencil) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        ClearFrameBufferInternal(0, MAX_COLOR_ATTACHMENTS, r, g, b, a, &depth, &stencil);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, "Invalid frame buffer ID");
    
    const size_t frameBufferIdx = static_cast<size_t>(framebufferID);
    FrameBuffer& frameBuffer = m_frameBuffers[frameBufferIdx];

    const uint32_t frameBufferRenderID = frameBuffer.GetRenderID();

    for (uint32_t index = 0; index < frameBuffer.GetColorAttachmentsCount(); ++index) {
        ClearFrameBufferColorInternal(frameBufferRenderID, index, r, g, b, a);
    }

    const uint32_t frameBufferDepthAttachmentsCount = frameBuffer.GetDepthAttachmentCount();
    const uint32_t frameBufferStencilAttachmentsCount = frameBuffer.GetStencilAttachmentCount();
    
    if (frameBufferDepthAttachmentsCount > 0 && frameBufferStencilAttachmentsCount > 0) {
        ClearFrameBufferDepthStencilInternal(frameBufferRenderID, depth, stencil);
        return;
    }

    if (frameBufferDepthAttachmentsCount > 0) {
        ClearFrameBufferDepthInternal(frameBufferRenderID, depth);
    }

    if (frameBufferStencilAttachmentsCount > 0) {
        ClearFrameBufferStencilInternal(frameBufferRenderID, stencil);
    }
}


void RenderTargetManager::ClearFrameBufferColor(RTFrameBufferID framebufferID, uint32_t index, float r, float g, float b, float a) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        ClearFrameBufferColorInternal(0, index, r, g, b, a);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, "Invalid frame buffer ID");
    
    const size_t frameBufferIdx = static_cast<size_t>(framebufferID);
    FrameBuffer& frameBuffer = m_frameBuffers[frameBufferIdx];

    if (index < frameBuffer.GetColorAttachmentsCount()) {
        ClearFrameBufferColorInternal(frameBuffer.GetRenderID(), index, r, g, b, a);
    }
}


void RenderTargetManager::ClearFrameBufferDepth(RTFrameBufferID framebufferID, float depth) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        ClearFrameBufferDepthInternal(0, depth);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, "Invalid frame buffer ID");
    
    const size_t frameBufferIdx = static_cast<size_t>(framebufferID);
    FrameBuffer& frameBuffer = m_frameBuffers[frameBufferIdx];

    if (frameBuffer.GetDepthAttachmentCount() > 0) {
        ClearFrameBufferDepthInternal(frameBuffer.GetRenderID(), depth);
    }
}

void RenderTargetManager::ClearFrameBufferStencil(RTFrameBufferID framebufferID, int32_t stencil) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        ClearFrameBufferStencilInternal(0, stencil);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, "Invalid frame buffer ID");
    
    const size_t frameBufferIdx = static_cast<size_t>(framebufferID);
    FrameBuffer& frameBuffer = m_frameBuffers[frameBufferIdx];

    if (frameBuffer.GetDepthAttachmentCount() > 0) {
        ClearFrameBufferStencilInternal(frameBuffer.GetRenderID(), stencil);
    }
}

void RenderTargetManager::ClearFrameBufferDepthStencil(RTFrameBufferID framebufferID, float depth, int32_t stencil) noexcept
{
    if (framebufferID == RTFrameBufferID::RT_FRAMEBUFFER_DEFAULT) {
        ClearFrameBufferDepthStencilInternal(0, depth, stencil);
        return;
    }

    ENG_ASSERT_GRAPHICS_API(framebufferID != RTFrameBufferID::RT_FRAMEBUFFER_INVALID, "Invalid frame buffer ID");
    
    const size_t frameBufferIdx = static_cast<size_t>(framebufferID);
    FrameBuffer& frameBuffer = m_frameBuffers[frameBufferIdx];

    if (frameBuffer.GetDepthAttachmentCount() > 0 && frameBuffer.GetStencilAttachmentCount() > 0) {
        ClearFrameBufferDepthStencilInternal(frameBuffer.GetRenderID(), depth, stencil);
    }
}


bool RenderTargetManager::Init() noexcept
{
    if (IsInitialized()) {
        return true;
    }

    EventDispatcher& dispatcher = EventDispatcher::GetInstance();

    dispatcher.Subscribe(EventListener::Create<EventFramebufferResized>([this](const void* pEvent) {
            const EventFramebufferResized& event = CastEventTo<EventFramebufferResized>(pEvent);
            const uint32_t width = event.GetWidth();
            const uint32_t height = event.GetHeight();

            if (width > 0 && height > 0) {
                OnWindowResizedEvent(width, height);
            }
        }
    ));

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);

    m_isInitialized = true;

    return true;
}


void RenderTargetManager::Terminate() noexcept
{
    ClearFrameBuffersStorage();

    m_isInitialized = false;
}


void RenderTargetManager::ClearFrameBuffersStorage() noexcept
{
    TextureManager& texManager = TextureManager::GetInstance();

    for (Texture* pTex : m_RTTextures) {
        texManager.DeallocateTexture(pTex->GetName());
    }
    m_RTTextures.clear();

    for (FrameBuffer& framebuffer : m_frameBuffers) {
        framebuffer.Destroy();
    }
    m_frameBuffers.clear();
}


void RenderTargetManager::OnWindowResizedEvent(uint32_t width, uint32_t height) noexcept
{
    RecreateFrameBuffers(width, height);
}


void RenderTargetManager::RecreateFrameBuffers(uint32_t width, uint32_t height) noexcept
{
    ClearFrameBuffersStorage();

    TextureManager& texManager = TextureManager::GetInstance();

    m_RTTextures.resize(static_cast<size_t>(RTTextureID::RT_TEX_COUNT));

    FrameBufferAttachment gbufferAlbedoColorAttachment = {};

    {
        Texture2DCreateInfo gbufferAlbedoTextureCreateInfo = {};
        gbufferAlbedoTextureCreateInfo.format = resGetTexResourceFormat(GBUFFER_ALBEDO_TEX);
        gbufferAlbedoTextureCreateInfo.width = width;
        gbufferAlbedoTextureCreateInfo.height = height;
        gbufferAlbedoTextureCreateInfo.mipmapsCount = 0;
        gbufferAlbedoTextureCreateInfo.inputData = {};
        
        ds::StrID gbufferAlbedoTexName = "__GBUFFER_ALBEDO__";
        TextureID gbufferAlbedoTexID = texManager.AllocateTexture2D(gbufferAlbedoTexName, gbufferAlbedoTextureCreateInfo);
        Texture* pGbufferAlbedoTex = texManager.GetTextureByID(gbufferAlbedoTexID);
        ENG_ASSERT_GRAPHICS_API(pGbufferAlbedoTex, "Failed to allocate \'{}\' texture", gbufferAlbedoTexName.CStr());
        
        const size_t gbufferAlbedoTexIdx = static_cast<size_t>(RTTextureID::RT_TEX_GBUFFER_ALBEDO);
        m_RTTextures[gbufferAlbedoTexIdx] = pGbufferAlbedoTex;

        gbufferAlbedoColorAttachment.pTexure = pGbufferAlbedoTex;
        gbufferAlbedoColorAttachment.type = FrameBufferAttachmentType::TYPE_COLOR_ATTACHMENT;
        gbufferAlbedoColorAttachment.index = 0;
    }

    FrameBufferAttachment gbufferNormalColorAttachment = {};

    {
        Texture2DCreateInfo gbufferNormalTextureCreateInfo = {};
        gbufferNormalTextureCreateInfo.format = resGetTexResourceFormat(GBUFFER_NORMAL_TEX);
        gbufferNormalTextureCreateInfo.width = width;
        gbufferNormalTextureCreateInfo.height = height;
        gbufferNormalTextureCreateInfo.mipmapsCount = 0;
        gbufferNormalTextureCreateInfo.inputData = {};

        ds::StrID gbufferNormalTexName = "__GBUFFER_NORMAL__";
        TextureID gbufferNormalTexID = texManager.AllocateTexture2D(gbufferNormalTexName, gbufferNormalTextureCreateInfo);
        Texture* pGbufferNormalTex = texManager.GetTextureByID(gbufferNormalTexID);
        ENG_ASSERT_GRAPHICS_API(pGbufferNormalTex, "Failed to allocate \'{}\' texture", gbufferNormalTexName.CStr());
        
        const size_t gbufferNormalTexIdx = static_cast<size_t>(RTTextureID::RT_TEX_GBUFFER_NORMAL);
        m_RTTextures[gbufferNormalTexIdx] = pGbufferNormalTex;

        gbufferNormalColorAttachment.pTexure = pGbufferNormalTex;
        gbufferNormalColorAttachment.type = FrameBufferAttachmentType::TYPE_COLOR_ATTACHMENT;
        gbufferNormalColorAttachment.index = 1;
    }

    FrameBufferAttachment gbufferSpecColorAttachment = {};

    {
        Texture2DCreateInfo gbufferSpecularTextureCreateInfo = {};
        gbufferSpecularTextureCreateInfo.format = resGetTexResourceFormat(GBUFFER_SPECULAR_TEX);
        gbufferSpecularTextureCreateInfo.width = width;
        gbufferSpecularTextureCreateInfo.height = height;
        gbufferSpecularTextureCreateInfo.mipmapsCount = 0;
        gbufferSpecularTextureCreateInfo.inputData = {};

        ds::StrID gbufferSpecularTexName = "__GBUFFER_SPECULAR__";
        TextureID gbufferSpecularTexID = texManager.AllocateTexture2D(gbufferSpecularTexName, gbufferSpecularTextureCreateInfo);
        Texture* pGbufferSpecTex = texManager.GetTextureByID(gbufferSpecularTexID);
        ENG_ASSERT_GRAPHICS_API(pGbufferSpecTex, "Failed to allocate \'{}\' texture", gbufferSpecularTexName.CStr());
        
        const size_t gbufferSpecTexIdx = static_cast<size_t>(RTTextureID::RT_TEX_GBUFFER_SPECULAR);
        m_RTTextures[gbufferSpecTexIdx] = pGbufferSpecTex;

        gbufferSpecColorAttachment.pTexure = pGbufferSpecTex;
        gbufferSpecColorAttachment.type = FrameBufferAttachmentType::TYPE_COLOR_ATTACHMENT;
        gbufferSpecColorAttachment.index = 2;
    }

    FrameBufferAttachment commonDepthAttachment = {};

    {
        Texture2DCreateInfo commonDepthTextureCreateInfo = {};
        commonDepthTextureCreateInfo.format = resGetTexResourceFormat(COMMON_DEPTH_TEX);
        commonDepthTextureCreateInfo.width = width;
        commonDepthTextureCreateInfo.height = height;
        commonDepthTextureCreateInfo.mipmapsCount = 0;
        commonDepthTextureCreateInfo.inputData = {};

        ds::StrID commonDepthTexName = "__COMMON_DEPTH__";
        TextureID commonDepthTexID = texManager.AllocateTexture2D(commonDepthTexName, commonDepthTextureCreateInfo);
        Texture* pCommonDepthTex = texManager.GetTextureByID(commonDepthTexID);
        ENG_ASSERT_GRAPHICS_API(pCommonDepthTex, "Failed to allocate \'{}\' texture", commonDepthTexName.CStr());
        
        const size_t commonDepthTexIdx = static_cast<size_t>(RTTextureID::RT_TEX_COMMON_DEPTH);
        m_RTTextures[commonDepthTexIdx] = pCommonDepthTex;

        commonDepthAttachment.pTexure = pCommonDepthTex;
        commonDepthAttachment.type = FrameBufferAttachmentType::TYPE_DEPTH_ATTACHMENT;
    }

    m_frameBuffers.resize(static_cast<size_t>(RTFrameBufferID::RT_FRAMEBUFFER_COUNT));

    {
        FramebufferCreateInfo gbufferFrameBufferCreateInfo = {};
        gbufferFrameBufferCreateInfo.ID = RTFrameBufferID::RT_FRAMEBUFFER_GBUFFER;

        FrameBufferAttachment pAttachments[] = { 
            gbufferAlbedoColorAttachment, gbufferNormalColorAttachment, gbufferSpecColorAttachment//, commonDepthAttachment
        };

        gbufferFrameBufferCreateInfo.pAttachments = pAttachments;
        gbufferFrameBufferCreateInfo.attachmentsCount = _countof(pAttachments);

        ds::StrID gbufferFrameBufferName = "__GBUFFER_FRAMEBUFFER__";
        const size_t gbufferFrameBufferIdx = static_cast<size_t>(RTFrameBufferID::RT_FRAMEBUFFER_GBUFFER);
        
        if (!m_frameBuffers[gbufferFrameBufferIdx].Init(gbufferFrameBufferName, gbufferFrameBufferCreateInfo)) {
            ENG_ASSERT_GRAPHICS_API_FAIL("Failed to initialize \'{}\' frame buffer", gbufferFrameBufferName.CStr());
        }
    }
}

void RenderTargetManager::ClearFrameBufferInternal(uint32_t frameBufferRenderID, uint32_t colorAttachmentsCount, float r, float g, float b, float a, const float *depth, const int32_t *stencil) noexcept
{
    for (uint32_t index = 0; index < colorAttachmentsCount; ++index) {
        ClearFrameBufferColorInternal(frameBufferRenderID, index, r, g, b, a);
    }

    if (depth && stencil) {
        ClearFrameBufferDepthStencilInternal(frameBufferRenderID, *depth, *stencil);
        return;
    }

    if (depth) {
        ClearFrameBufferDepthInternal(frameBufferRenderID, *depth);
    }

    if (stencil) {
        ClearFrameBufferStencilInternal(frameBufferRenderID, *stencil);
    }
}


void RenderTargetManager::ClearFrameBufferColorInternal(uint32_t frameBufferRenderID, uint32_t index, float r, float g, float b, float a) noexcept
{
    const float pClearColor[] = { r, g, b, a };
    glClearNamedFramebufferfv(frameBufferRenderID, GL_COLOR, index, pClearColor);
}


void RenderTargetManager::ClearFrameBufferDepthInternal(uint32_t frameBufferRenderID, float depth) noexcept
{
    glClearNamedFramebufferfv(frameBufferRenderID, GL_DEPTH, 0, &depth);
}


void RenderTargetManager::ClearFrameBufferStencilInternal(uint32_t frameBufferRenderID, int32_t stencil) noexcept
{
    glClearNamedFramebufferiv(frameBufferRenderID, GL_STENCIL, 0, &stencil);
}


void RenderTargetManager::ClearFrameBufferDepthStencilInternal(uint32_t frameBufferRenderID, float depth, int32_t stencil) noexcept
{
    glClearNamedFramebufferfi(frameBufferRenderID, GL_DEPTH_STENCIL, 0, depth, stencil);
}


bool engInitRenderTargetManager() noexcept
{
    if (engIsRenderTargetManagerInitialized()) {
        ENG_LOG_GRAPHICS_API_WARN("Render target manager is already initialized!");
        return true;
    }

    ENG_ASSERT_GRAPHICS_API(engIsTextureManagerInitialized(), "Texture manager must be initialized before render target manager!");

    s_pRenderTargetMng = std::unique_ptr<RenderTargetManager>(new RenderTargetManager);

    if (!s_pRenderTargetMng) {
        ENG_ASSERT_GRAPHICS_API_FAIL("Failed to allocate memory for render target manager");
        return false;
    }

    if (!s_pRenderTargetMng->Init()) {
        ENG_ASSERT_GRAPHICS_API_FAIL("Failed to initialized render target manager");
        return false;
    }

    return true;
}


void engTerminateRenderTargetManager() noexcept
{
    s_pRenderTargetMng = nullptr;
}


bool engIsRenderTargetManagerInitialized() noexcept
{
    return s_pRenderTargetMng && s_pRenderTargetMng->IsInitialized();
}
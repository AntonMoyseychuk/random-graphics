#ifndef OPENGL_DRIVER_H
#define OPENGL_DRIVER_H

#include <glad/glad.h>


bool engInitOpenGLDriver() noexcept;
bool engIsOpenGLDriverInitialized() noexcept;


#undef glUseProgram
void drvBindShaderProgram(GLuint programID) noexcept;
#define glUseProgram drvBindShaderProgram

#undef glBindVertexArray
void drvBindVertexArray(GLuint arrayID) noexcept;
#define glBindVertexArray drvBindVertexArray

#undef glBindTextureUnit
void drvBindTextureUnit(GLuint unit, GLuint textureID) noexcept;
#define glBindTextureUnit drvBindTextureUnit

void drvUnbindTextureUnit(GLuint unit, GLuint textureID) noexcept;

#endif
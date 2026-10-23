#pragma once
#include <epoxy/gl.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

namespace chaos_ui {
inline void beginOpenGLFrame(){
    if(ImGui::GetIO().Fonts->TexID){ImGui_ImplOpenGL3_NewFrame();return;}
    GLint buffer,alignment,rowLength,skipPixels,skipRows;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    ImGui_ImplOpenGL3_NewFrame();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);
}
}

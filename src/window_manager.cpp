#include "window_manager.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

void error_callback(int error, const char* description)
{
  std::cerr << "glfw error " << error << " : " << description << std::endl;
}
WindowManager::WindowManager(int width, int height, const char* title, bool fullscreen) : width(width), height(height)
{
  glfwSetErrorCallback(error_callback);
  if (!glfwInit())                       // <-- was missing
        throw std::runtime_error("Failed to init GLFW");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  //glfwWindowHint(GLFW_DECORATED,GLFW_FALSE);

  GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
  window = glfwCreateWindow(width, height, title, monitor, nullptr);

  if (!window)
  {
    glfwTerminate();
    throw std::runtime_error("Failed to create window");
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGL()) {
    glfwDestroyWindow(window);
    glfwTerminate();
    throw std::runtime_error("Failed to load GL");
  }
  // vsync 
  glfwSwapInterval(1);
  //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetKeyCallback(window, keyCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);            // initial viewport
}


WindowManager::~WindowManager()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  if (window){glfwDestroyWindow(window);}
  glfwTerminate();
}

void WindowManager::beginImGuiFrame() const  
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}
void WindowManager::renderImGui() const
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
bool WindowManager::shouldClose() const { return glfwWindowShouldClose(window); }
void WindowManager::pollEvents() const {  glfwPollEvents(); }
void WindowManager::swapBuffers() const {  glfwSwapBuffers(window); }
void WindowManager::framebufferSizeCallback(GLFWwindow* window, int width, int height) 
{
  glViewport(0,0,width,height);
}

void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}



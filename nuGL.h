#ifndef NUGL_H
#define NUGL_H

// Includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structs
typedef struct {
  GLFWwindow *window;
  size_t width;  // Width of window in pixels
  size_t height; // Height of window in pixels
  bool active;   // Boolean checking if the window is valid
  // Could add keys and stuff
} nu_Window;

typedef struct {
  char *data;
  size_t bytes_alloced;
  size_t bytes_added;
  size_t stride;
  unsigned int VAO, VBO;
  GLenum render_mode; // GL_TRIANGLES by default
} nu_Mesh;

typedef struct {
  GLuint shader_program;
  // Could add things like a list of uniform locations here
} nu_Program;

typedef struct {
  GLuint id;
  GLenum type;
} nu_Texture;

// Function prototypes
// Calls glfwTerminate() to free GLFW resources
void nu_terminate(void);
// Enables / disables vsync
void nu_set_vsync(bool state);
// Returns a window with the given name, width, and height. If fullscreen is
// true, the window will be fullscreen
nu_Window nu_create_window(const char *name, size_t width, size_t height,
                           bool fullscreen);
// Destroys a window
void nu_destroy_window(nu_Window *window);
void nu_start_frame(nu_Window *window);
void nu_end_frame(nu_Window *window);
// Creates a shader program from a list of shader directories. It will detect
// the shader type from the file extension. Check C source file for all
// extensions
nu_Program nu_create_program(size_t num_shaders, ...);
// Sets a shader program as the currently used one
void nu_use_program(nu_Program *program);
// Deletes a shader program
void nu_destroy_program(nu_Program *program);
// Creates a mesh for a specified vertex layout.
// num_components = the number of attributes
// component_sizes = the size of the individual types of each attribute
// component_counts = the number of each attribute (vec3 = 3, float = 1)
// component_types = the individual type of each attribute (vec3 = GL_FLOAT,
// float = GL_FLOAT, int = GL_INT)
nu_Mesh nu_create_mesh(size_t num_components, size_t *component_sizes,
                       size_t *component_counts, GLenum *component_types);
// Add a number of bytes to a mesh from a pointer to the source
void nu_mesh_add_bytes(nu_Mesh *mesh, void *src, size_t n_bytes);
// Free CPU-side resources of a mesh, keep its VAO and VBO intact
void nu_free_mesh(nu_Mesh *mesh);
// Free CPU and GPU resources of a mesh, delete its VAO and VBO.
void nu_destroy_mesh(nu_Mesh *mesh);
// Send the bytes added to a mesh to its VBO
void nu_send_mesh(nu_Mesh *mesh);
// Render a mesh
void nu_render_mesh(nu_Mesh *mesh);
// Switch the render mode of a mesh
void nu_set_render_mode(nu_Mesh *mesh, GLenum mode);
// Load a texture from an image file path
nu_Texture nu_load_texture(const char *file_loc);
// Load a texture array from a list of image file paths (char *'s)
nu_Texture nu_load_texture_array(size_t num_textures, ...);
// Delete a texture / texture array
void nu_destroy_texture(nu_Texture *texture);
// Bind a number of textures to slots 0 -> num_textures - 1 (nu_Texture *'s)
void nu_use_textures(size_t num_textures, ...);
// Bind a specific texture to a specific slot
void nu_bind_texture(nu_Texture *texture, size_t slot);
// Single variable uniforms
void nu_send_uniform_int(int val, nu_Program *program,
                         const char *uniform_name);
void nu_send_uniform_float(float val, nu_Program *program,
                           const char *uniform_name);
// Vector uniforms
void nu_send_uniform_vec2(float *val, nu_Program *program,
                          const char *uniform_name);
void nu_send_uniform_vec3(float *val, nu_Program *program,
                          const char *uniform_name);
void nu_send_uniform_vec4(float *val, nu_Program *program,
                          const char *uniform_name);
// Matrix uniforms
void nu_send_uniform_mat4(float *val, nu_Program *program,
                          const char *uniform_name);

#endif // nuGL.h

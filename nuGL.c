#include "nuGL.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb/stb_image.h"

void nu_terminate(void) { glfwTerminate(); }

void window_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

nu_Window nu_create_window(const char *name, size_t width, size_t height,
                           bool fullscreen) {
  nu_Window res = {NULL, 0, 0, false};
  if (glfwInit() != GLFW_TRUE) {
    fprintf(stderr, "(nu_init): Error initialising GLFW.\n");
    return res;
  }
  if (fullscreen)
    res.window =
        glfwCreateWindow(width, height, name, glfwGetPrimaryMonitor(), NULL);
  else
    res.window = glfwCreateWindow(width, height, name, NULL, NULL);
  if (res.window == NULL) {
    fprintf(stderr, "(nu_create_window) Error creating window, "
                    "glfwCreateWindow returned NULL.\n");
    res.active = false;
    return res;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwMakeContextCurrent(res.window);
  res.width = width;
  res.height = height;
  res.active = true;

  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "(nu_init): Error initialising GLEW.\n");
    glfwTerminate();
    res.window = NULL;
    res.active = false;
    return res;
  }
  // TODO: figure out how to use key callbacks for the window
  glfwSetWindowSizeCallback(res.window, window_size_callback);
  return res;
}

void nu_destroy_window(nu_Window *window) {
  if (!window->active)
    return;
  if (window->window)
    glfwDestroyWindow(window->window);
  window->window = NULL;
  window->active = false;
}

void nu_start_frame(nu_Window *window) {
  if (!window->active)
    return;
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void nu_end_frame(nu_Window *window) {
  if (!window->active)
    return;
  glfwSwapBuffers(window->window);
  glfwPollEvents();
}

// Read a shader file, and return as a string
static char *nu_read_file(const char *file_name) {
  FILE *ptr = fopen(file_name, "rb"); // Open the file for reading
  if (ptr == NULL) {
    fprintf(stderr, "(nu_read_file): Could not open file %s, returning NULL.\n",
            file_name);
    return NULL;
  }
  size_t file_len = 0;
  fseek(ptr, 0, SEEK_END);
  file_len = ftell(ptr);
  if (file_len == 0) {
    fclose(ptr);
    fprintf(stderr,
            "(nu_read_file): Could not read file %s, file has 0 length.",
            file_name);
    return NULL;
  }

  // Allocate output
  char *out = malloc(sizeof(char) * file_len + 1);

  // Read the file into output
  fseek(ptr, 0, SEEK_SET);
  if (fread(out, 1, file_len, ptr) != file_len) {
    fprintf(stderr, "(nu_read_file): Could not read file %s, fread() failed.\n",
            file_name);
    free(out);
    fclose(ptr);
    return NULL;
  }
  // Null terminate
  out[file_len] = '\0';
  // Close file
  fclose(ptr);
  return out;
}

static GLuint nu_compile_shader(const char *shader_file_location) {
  if (!shader_file_location)
    return 0;
  // Detect the shader type based on file extension
  size_t name_len = strlen(shader_file_location);
  if (name_len < 5) {
    fprintf(stderr, "(nu_compile_shader): Invalid file name %s\n");
  }
  const char *ext = shader_file_location + name_len - 5;

  GLenum shader_types[] = {GL_VERTEX_SHADER,          GL_FRAGMENT_SHADER,
                           GL_GEOMETRY_SHADER,        GL_TESS_CONTROL_SHADER,
                           GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};
  const char *shader_exts[] = {".vert", ".frag", ".geom",
                               ".tesc", ".tese", ".comp"};
  size_t num_types = sizeof(shader_types) / sizeof(shader_types[0]);

  GLenum shader_type = 0;
  for (size_t i = 0; i < num_types; i++) {
    if (strcmp(ext, shader_exts[i]) == 0) {
      shader_type = shader_types[i];
      break;
    }
  }
  if (shader_type == 0) {
    fprintf(stderr, "(nu_compile_shader): Unsupported extension in %s\n",
            shader_file_location);
    fprintf(stderr, "expected:\n");
    for (size_t i = 0; i < num_types; i++) {
      fprintf(stderr, "%s\n", shader_exts[i]);
    }
    return 0;
  }

  // Read the shader file's contents into a string
  char *source = nu_read_file(shader_file_location);

  if (!source) {
    fprintf(stderr, "(nu_compile_shader): Failed to read shader file %s\n",
            shader_file_location);
    return 0;
  }
  size_t src_len;
  src_len = strlen(source);

  // Create and compile the shader
  GLuint shader = glCreateShader(shader_type);
  const GLchar *src = source;
  GLint len = (GLint)src_len;
  glShaderSource(shader, 1, &src, &len);
  free(source);

  glCompileShader(shader);

  // Check for compilation errors
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "(nu_compile_shader): Compilation failed for %s:\n%s",
            shader_file_location, log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

nu_Program nu_create_program(size_t num_shaders, ...) {
  nu_Program res = {0};
  va_list args;
  va_start(args, num_shaders);

  GLuint *shaders = malloc(sizeof(unsigned int) * num_shaders);
  for (size_t i = 0; i < num_shaders; i++) {
    // Create and compile each shader
    char *shader_file_name = va_arg(args, char *);
    GLuint shader = nu_compile_shader(shader_file_name);
    // Check for compilation error
    if (shader == 0) {
      fprintf(stderr,
              "(nu_create_shader_program): Couldn't create shader program, "
              "compilation of shader %s failed.\n",
              shader_file_name);
      // Delete all other shaders on failure to compile
      for (size_t j = 0; j < i; j++) {
        glDeleteShader(shaders[j]);
      }
      free(shaders);
      va_end(args);
      return res;
    }

    shaders[i] = shader;
  }
  va_end(args);

  // Create the program
  GLuint shader_program = glCreateProgram();
  for (size_t i = 0; i < num_shaders; i++) {
    glAttachShader(shader_program, shaders[i]);
  }

  glLinkProgram(shader_program);
  // Delete all shaders now, stack overflow says i can do it lol
  for (size_t i = 0; i < num_shaders; i++) {
    glDeleteShader(shaders[i]);
  }
  free(shaders);

  // Check for linking errors
  GLint success;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetProgramInfoLog(shader_program, sizeof(log), NULL, log);
    fprintf(stderr, "(nu_create_shader_program): Shader linking failed:\n%s",
            log);
    glDeleteProgram(shader_program);
    return res;
  }

  res.shader_program = shader_program;
  return res;
}

void nu_use_program(nu_Program *program) {
  if (!program)
    return;
  glUseProgram(program->shader_program);
}

void nu_destroy_program(nu_Program *program) {
  if (!program)
    return;
  glDeleteProgram(program->shader_program);
  program->shader_program = 0;
}

static void nu_define_layout(GLuint *VAO, GLuint *VBO, size_t num_components,
                             size_t *component_sizes, size_t *component_counts,
                             GLenum *component_types) {
  // Clear whatever might exist in the VAO and VBO
  glDeleteBuffers(1, VBO);
  glGenBuffers(1, VBO);
  glBindBuffer(GL_ARRAY_BUFFER, *VBO);

  glDeleteVertexArrays(1, VAO);
  glGenVertexArrays(1, VAO);
  glBindVertexArray(*VAO);

  // Calculate stride
  size_t stride = 0;
  for (size_t i = 0; i < num_components; i++) {
    stride += component_sizes[i] * component_counts[i];
  }
  // Attrib pointer to each component
  size_t offset = 0;
  for (size_t i = 0; i < num_components; i++) {
    glVertexAttribPointer(i, component_counts[i], component_types[i], GL_FALSE,
                          stride, (GLvoid *)(intptr_t)offset);
    glEnableVertexAttribArray(i);
    offset += component_sizes[i] * component_counts[i];
  }
}

nu_Mesh nu_create_mesh(size_t num_components, size_t *component_sizes,
                       size_t *component_counts, GLenum *component_types) {
  nu_Mesh mesh = {0};
  mesh.data = NULL;
  mesh.bytes_alloced = 0;
  mesh.bytes_added = 0;
  mesh.stride = 0;
  mesh.render_mode = GL_TRIANGLES;
  for (size_t i = 0; i < num_components; i++) {
    mesh.stride += component_sizes[i] * component_counts[i];
  }

  nu_define_layout(&mesh.VAO, &mesh.VBO, num_components, component_sizes,
                   component_counts, component_types);
  return mesh;
}

void nu_mesh_add_bytes(nu_Mesh *mesh, void *src, size_t n_bytes) {
  if (n_bytes == 0)
    return;
  if (!src)
    return;
  if (!mesh)
    return;

  if (!mesh->data) {
    mesh->bytes_alloced = 1024;
    mesh->data = malloc(sizeof(uint8_t) * mesh->bytes_alloced);
    mesh->bytes_added = 0;
  }

  if (mesh->bytes_added + n_bytes > mesh->bytes_alloced) {
    while (mesh->bytes_added + n_bytes > mesh->bytes_alloced) {
      mesh->bytes_alloced *= 2;
    }
    mesh->data = realloc(mesh->data, mesh->bytes_alloced * sizeof(uint8_t));
  }

  memcpy(mesh->data + mesh->bytes_added, src, n_bytes);
  mesh->bytes_added += n_bytes;
}

void nu_free_mesh(nu_Mesh *mesh) {
  if (!mesh)
    return;
  if (mesh->data)
    free(mesh->data);
  mesh->data = NULL;
  mesh->bytes_alloced = 0;
  // dont reset mesh->bytes_added, nu_mesh_add_bytes will do that for us
}

void nu_destroy_mesh(nu_Mesh *mesh) {
  if (!mesh)
    return;
  nu_free_mesh(mesh);
  glDeleteVertexArrays(1, &(mesh->VAO));
  glDeleteBuffers(1, &(mesh->VBO));
}

static void nu_bind_mesh(nu_Mesh *mesh) {
  if (!mesh)
    return;
  glBindVertexArray(mesh->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
}

static void nu_unbind_mesh() {
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void nu_send_mesh(nu_Mesh *mesh) {
  if (!mesh)
    return;
  if (!mesh->data)
    return;
  nu_bind_mesh(mesh);
  glBufferData(GL_ARRAY_BUFFER, mesh->bytes_added, mesh->data, GL_STATIC_DRAW);
  nu_unbind_mesh();
}

void nu_render_mesh(nu_Mesh *mesh) {
  nu_bind_mesh(mesh);
  glDrawArrays(mesh->render_mode, 0, mesh->bytes_added / mesh->stride);
  nu_unbind_mesh();
}

nu_Texture nu_load_texture(const char *file_loc) {
  nu_Texture res;
  res.id = 0;
  res.type = GL_TEXTURE_2D;

  int image_width, image_height, comp; // No idea what comp is
  stbi_set_flip_vertically_on_load(1);
  char *image =
      stbi_load(file_loc, &image_width, &image_height, &comp, STBI_rgb_alpha);

  if (image == NULL) {
    fprintf(stderr,
            "(nu_create_texture): Error loading image %s, stbi_load returned "
            "NULL.\n",
            file_loc);
    return res;
  }
  // Create and bind the texture
  glGenTextures(1, &res.id);
  glBindTexture(GL_TEXTURE_2D, res.id);

  // Set parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Upload the loaded image to the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image);

  // Free image
  stbi_image_free(image);
  return res;
}

nu_Texture nu_load_texture_array(size_t num_textures, ...) {
  nu_Texture res;
  res.id = 0;
  res.type = GL_TEXTURE_2D_ARRAY;
  if (num_textures == 0)
    return res;

  va_list args;
  va_start(args, num_textures);

  stbi_set_flip_vertically_on_load(1);

  char *first_path = va_arg(args, char *);
  int width, height, comp;
  unsigned char *first_image =
      stbi_load(first_path, &width, &height, &comp, STBI_rgb_alpha);
  if (!first_image) {
    fprintf(stderr, "(nu_load_texture_array): Failed to load first image %s\n",
            first_path);
    va_end(args);
    return res;
  }

  glGenTextures(1, &res.id);
  glBindTexture(GL_TEXTURE_2D_ARRAY, res.id);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height,
               (GLsizei)num_textures, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, 1, GL_RGBA,
                  GL_UNSIGNED_BYTE, first_image);

  stbi_image_free(first_image);

  for (size_t i = 1; i < num_textures; i++) {
    char *path = va_arg(args, char *);

    int w, h, c;
    unsigned char *image = stbi_load(path, &w, &h, &c, STBI_rgb_alpha);
    if (!image) {
      fprintf(stderr, "(nu_load_texture_array): Failed to load image %s\n",
              path);
      glDeleteTextures(1, &res.id);
      va_end(args);
      return res;
    }

    if (w != width || h != height) {
      fprintf(stderr,
              "(nu_load_texture_array): Image %s does not match size %dx%d, "
              "skipping.\n",
              path, width, height);
      stbi_image_free(image);
      continue;
    }

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)i, width, height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, image);

    stbi_image_free(image);
  }

  va_end(args);
  return res;
}

void nu_destroy_texture(nu_Texture *texture) {
  if (!texture)
    return;
  glDeleteTextures(1, &texture->id);
  texture->id = 0;
}

void nu_use_textures(size_t num_textures, ...) {
  va_list args;
  va_start(args, num_textures);
  for (size_t i = 0; i < num_textures; i++) {
    nu_Texture *tex = va_arg(args, nu_Texture *);
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(tex->type, tex->id);
  }
  va_end(args);
}

static bool nu_get_uniform_loc(nu_Program *program, const char *uniform_name,
                               GLuint *res) {
  if (!program)
    return false;
  glUseProgram(program->shader_program);
  GLuint uniform_loc =
      glGetUniformLocation(program->shader_program, uniform_name);
  if (uniform_loc == -1) {
    fprintf(stderr,
            "(nu_get_uniform_loc) No uniform \"%s\" found in shader program.\n",
            uniform_name);
    return false;
  }
  *res = uniform_loc;
  return true;
}

void nu_send_uniform_int(int val, nu_Program *program,
                         const char *uniform_name) {
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniform1i(uniform_loc, val);
  glUseProgram(0);
}

void nu_send_uniform_float(float val, nu_Program *program,
                           const char *uniform_name) {
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniform1f(uniform_loc, val);
  glUseProgram(0);
}

void nu_send_uniform_vec2(float *val, nu_Program *program,
                          const char *uniform_name) {
  if (!val)
    return;
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniform2f(uniform_loc, val[0], val[1]);
  glUseProgram(0);
}

void nu_send_uniform_vec3(float *val, nu_Program *program,
                          const char *uniform_name) {
  if (!val)
    return;
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniform3f(uniform_loc, val[0], val[1], val[2]);
  glUseProgram(0);
}

void nu_send_uniform_vec4(float *val, nu_Program *program,
                          const char *uniform_name) {
  if (!val)
    return;
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniform4f(uniform_loc, val[0], val[1], val[2], val[3]);
  glUseProgram(0);
}

void nu_send_uniform_mat4(float *val, nu_Program *program,
                          const char *uniform_name) {
  if (!val)
    return;
  GLuint uniform_loc;
  if (!nu_get_uniform_loc(program, uniform_name, &uniform_loc))
    return;
  glUniformMatrix4fv(uniform_loc, 1, GL_FALSE, val);
  glUseProgram(0);
}

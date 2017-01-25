#include "esUtil.h"

typedef struct {
  GLuint programObject;
  } UserData;

/*{{{*/
GLuint loadShader (GLenum type, const char* shaderSrc) {

  // Create the shader object
  GLuint shader = glCreateShader (type);
  if (shader == 0)
    return 0;

  // Load the shader source
  glShaderSource (shader, 1, &shaderSrc, NULL);

  // Compile the shader
  glCompileShader (shader);

  // Check the compile status
  GLint compiled;
  glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);

  if (!compiled) {
   GLint infoLen = 0;
   glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLen);

   if ( infoLen > 1 ) {
     char* infoLog = malloc (sizeof(char)* infoLen);
     glGetShaderInfoLog (shader, infoLen, NULL, infoLog);
     esLogMessage ("Error compiling shader:\n%s\n", infoLog);
     free (infoLog);
     }

    glDeleteShader (shader);
    return 0;
    }

  return shader;
  }
/*}}}*/

/*{{{*/
void draw (ESContext* esContext) {

  GLfloat vVertices[] = {  0.0f,  0.5f, 0.0f,
                          -0.5f, -0.5f, 0.0f,
                           0.5f, -0.5f, 0.0f };

  // Set the viewport
  glViewport (0, 0, esContext->width, esContext->height);

  // Clear the color buffer
  glClear (GL_COLOR_BUFFER_BIT);

  // Use the program object
  UserData* userData = esContext->userData;
  glUseProgram (userData->programObject);

  // Load the vertex data
  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
  glEnableVertexAttribArray (0);

  glDrawArrays (GL_TRIANGLES, 0, 3);
  }
/*}}}*/
/*{{{*/
void shutdown (ESContext* esContext) {

  UserData* userData = esContext->userData;
  glDeleteProgram (userData->programObject);
  }
/*}}}*/

/*{{{*/
int init (ESContext* esContext) {

  char vShaderStr[] =
     "#version 300 es                          \n"
     "layout(location = 0) in vec4 vPosition;  \n"
     "void main()                              \n"
     "{                                        \n"
     "   gl_Position = vPosition;              \n"
     "}                                        \n";

  char fShaderStr[] =
     "#version 300 es                              \n"
     "precision mediump float;                     \n"
     "out vec4 fragColor;                          \n"
     "void main()                                  \n"
     "{                                            \n"
     "   fragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );  \n"
     "}                                            \n";

  // Load the vertex/fragment shaders
  GLuint vertexShader = loadShader (GL_VERTEX_SHADER, vShaderStr);
  GLuint fragmentShader = loadShader (GL_FRAGMENT_SHADER, fShaderStr);

  // Create the program object
  GLuint programObject = glCreateProgram();
  if (programObject == 0)
    return 0;

  glAttachShader (programObject, vertexShader);
  glAttachShader (programObject, fragmentShader);

  // Link the program
  glLinkProgram (programObject);

  // Check the link status
  GLint linked;
  glGetProgramiv (programObject, GL_LINK_STATUS, &linked);

  if (!linked) {
     GLint infoLen = 0;
     glGetProgramiv (programObject, GL_INFO_LOG_LENGTH, &infoLen);
     if (infoLen > 1) {
       char *infoLog = malloc (sizeof(char)* infoLen);
       glGetProgramInfoLog (programObject, infoLen, NULL, infoLog);
       esLogMessage ("Error linking program:\n%s\n", infoLog);
       free (infoLog);
       }

     glDeleteProgram (programObject);
     return FALSE;
    }

  // Store the program object
  UserData* userData = esContext->userData;
  userData->programObject = programObject;

  glClearColor (1.0f, 1.0f, 1.0f, 0.0f);
  return TRUE;
  }
/*}}}*/

int esMain (ESContext* esContext ) {

   esContext->userData = malloc (sizeof(UserData));
   esCreateWindow (esContext, "Hello Triangle", 320, 240, ES_WINDOW_RGB);
   if (!init (esContext))
     return GL_FALSE;

   esRegisterShutdownFunc (esContext, shutdown);
   esRegisterDrawFunc (esContext, draw);

   return GL_TRUE;
  }

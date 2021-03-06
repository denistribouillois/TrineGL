#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <stack>

#include <cmath>

#include "glew/glew.h"

#include "GLFW/glfw3.h"
#include "stb/stb_image.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw_gl3.h"

#include "glm/glm.hpp"
#include "glm/vec3.hpp" // glm::vec3
#include "glm/vec4.hpp" // glm::vec4, glm::ivec4
#include "glm/mat4x4.hpp" // glm::mat4
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // glm::value_ptr

/* assimp include files. These three are usually needed. */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

#if DEBUG_PRINT == 0
#define debug_print(FORMAT, ...) ((void)0)
#else
#ifdef _MSC_VER
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __func__, __FILE__, __LINE__, __VA_ARGS__)
#endif
#endif

// Font buffers
extern const unsigned char DroidSans_ttf[];
extern const unsigned int DroidSans_ttf_len;    

// Shader utils
int check_link_error(GLuint program);
int check_compile_error(GLuint shader, const char ** sourceBuffer);
int check_link_error(GLuint program);
GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize);
GLuint compile_shader_from_file(GLenum shaderType, const char * fileName);

// OpenGL utils
bool checkError(const char* title);

struct Camera
{
    float radius;
    float theta;
    float phi;
    glm::vec3 o;
    glm::vec3 eye;
    glm::vec3 up;
};
void camera_defaults(Camera & c);
void camera_zoom(Camera & c, float factor);
void camera_turn(Camera & c, float phi, float theta);
void camera_pan(Camera & c, float x, float y);

struct GUIStates
{
    bool panLock;
    bool turnLock;
    bool zoomLock;
    int lockPositionX;
    int lockPositionY;
    int camera;
    double time;
    bool playing;
    static const float MOUSE_PAN_SPEED;
    static const float MOUSE_ZOOM_SPEED;
    static const float MOUSE_TURN_SPEED;
};
const float GUIStates::MOUSE_PAN_SPEED = 0.001f;
const float GUIStates::MOUSE_ZOOM_SPEED = 0.05f;
const float GUIStates::MOUSE_TURN_SPEED = 0.005f;
void init_gui_states(GUIStates & guiStates);




int main( int argc, char **argv )
{
    int width = 1024, height= 768;
    float widthf = (float) width, heightf = (float) height;
    double t;

    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GL_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    int const DPI = 2; // For retina screens only
#else
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    int const DPI = 1;
# endif

    // Open a window and create its OpenGL context
    GLFWwindow * window = glfwCreateWindow(width/DPI, height/DPI, "aogl", 0, 0);
    if( ! window )
    {
        fprintf( stderr, "Failed to open GLFW window\n" );
        glfwTerminate();
        exit( EXIT_FAILURE );
    }
    glfwMakeContextCurrent(window);

    // Init glew
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
          /* Problem: glewInit failed, something is seriously wrong. */
          fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
          exit( EXIT_FAILURE );
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode( window, GLFW_STICKY_KEYS, GL_TRUE );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );
    GLenum glerr = GL_NO_ERROR;
    glerr = glGetError();
    if (glerr != GL_NO_ERROR)
        std::cerr<<glerr;

    ImGui_ImplGlfwGL3_Init(window, true);

    // Init viewer structures
    Camera camera;
    camera_defaults(camera);
    GUIStates guiStates;
    init_gui_states(guiStates);
    float instanceCount = 2500;
    int pointLightCount = 0;
    int directionalLightCount = 1;
    //int spotLightCount = 0;
    float speed = 1.f;
    float gamma = 1.f;
    float factor = 0.1f;
    int sampleCount = 10.0;
    float focusPlane = 5.0;
    float nearPlane = 3.0;
    float farPlane = 14.0;

    float X = 0;
    float Y = 0;
    float Z = 0;
    float R = 0.5;
    float G = 0.5;
    float B = 0.5;
    float I = 1;

    // Load images and upload textures
    GLuint textures[2];
    glGenTextures(2, textures);
    int x;
    int y;
    int comp;

    unsigned char * diffuse = stbi_load("textures/spnza_bricks_a_diff.bmp", &x, &y, &comp, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    unsigned char * spec = stbi_load("textures/spnza_bricks_a_spec.tga", &x, &y, &comp, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, spec);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    checkError("Texture Initialization");

    // Try to load and compile blit shaders
    GLuint vertBlitShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "blit.vert");
    GLuint fragBlitShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "blit.frag");
    GLuint blitProgramObject = glCreateProgram();
    glAttachShader(blitProgramObject, vertBlitShaderId);
    glAttachShader(blitProgramObject, fragBlitShaderId);
    glLinkProgram(blitProgramObject);
    if (check_link_error(blitProgramObject) < 0)
        exit(1);
    GLuint blitTextureLocation = glGetUniformLocation(blitProgramObject, "Texture");
    glProgramUniform1i(blitProgramObject, blitTextureLocation, 0);

    // Try to load and compile gbuffer shaders
    GLuint vertgbufferShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "gbuffer.vert");
    GLuint fraggbufferShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "gbuffer.frag");
    GLuint gbufferProgramObject = glCreateProgram();
    glAttachShader(gbufferProgramObject, vertgbufferShaderId);
    glAttachShader(gbufferProgramObject, fraggbufferShaderId);
    glLinkProgram(gbufferProgramObject);
    if (check_link_error(gbufferProgramObject) < 0)
        exit(1);
    GLuint mvpLocation = glGetUniformLocation(gbufferProgramObject, "MVP");
    GLuint mvLocation = glGetUniformLocation(gbufferProgramObject, "MV");
    GLuint timeLocation = glGetUniformLocation(gbufferProgramObject, "Time");
    GLuint diffuseLocation = glGetUniformLocation(gbufferProgramObject, "Diffuse");
    GLuint specLocation = glGetUniformLocation(gbufferProgramObject, "Specular");
    GLuint specularPowerLocation = glGetUniformLocation(gbufferProgramObject, "SpecularPower");
    GLuint instanceCountLocation = glGetUniformLocation(gbufferProgramObject, "InstanceCount");
    glProgramUniform1i(gbufferProgramObject, diffuseLocation, 0);
    glProgramUniform1i(gbufferProgramObject, specLocation, 1);

    // Try to load and compile pointlight shaders
    GLuint fragpointlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "pointlight.frag");
    GLuint pointlightProgramObject = glCreateProgram();
    glAttachShader(pointlightProgramObject, vertBlitShaderId);
    glAttachShader(pointlightProgramObject, fragpointlightShaderId);
    glLinkProgram(pointlightProgramObject);
    if (check_link_error(pointlightProgramObject) < 0)
        exit(1);
    GLuint pointlightColorLocation = glGetUniformLocation(pointlightProgramObject, "ColorBuffer");
    GLuint pointlightNormalLocation = glGetUniformLocation(pointlightProgramObject, "NormalBuffer");
    GLuint pointlightDepthLocation = glGetUniformLocation(pointlightProgramObject, "DepthBuffer");
    GLuint pointlightLightLocation = glGetUniformBlockIndex(pointlightProgramObject, "light");
    GLuint pointInverseProjectionLocation = glGetUniformLocation(pointlightProgramObject, "InverseProjection");
    glProgramUniform1i(pointlightProgramObject, pointlightColorLocation, 0);
    glProgramUniform1i(pointlightProgramObject, pointlightNormalLocation, 1);
    glProgramUniform1i(pointlightProgramObject, pointlightDepthLocation, 2);

    // Try to load and compile directionallight shaders
    GLuint fragdirectionallightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "directionallight.frag");
    GLuint directionallightProgramObject = glCreateProgram();
    glAttachShader(directionallightProgramObject, vertBlitShaderId);
    glAttachShader(directionallightProgramObject, fragdirectionallightShaderId);
    glLinkProgram(directionallightProgramObject);
    if (check_link_error(directionallightProgramObject) < 0)
        exit(1);
    GLuint directionallightColorLocation = glGetUniformLocation(directionallightProgramObject, "ColorBuffer");
    GLuint directionallightNormalLocation = glGetUniformLocation(directionallightProgramObject, "NormalBuffer");
    GLuint directionallightDepthLocation = glGetUniformLocation(directionallightProgramObject, "DepthBuffer");
    GLuint directionallightLightLocation = glGetUniformBlockIndex(directionallightProgramObject, "light");
    GLuint directionalInverseProjectionLocation = glGetUniformLocation(directionallightProgramObject, "InverseProjection");
    glProgramUniform1i(directionallightProgramObject, directionallightColorLocation, 0);
    glProgramUniform1i(directionallightProgramObject, directionallightNormalLocation, 1);
    glProgramUniform1i(directionallightProgramObject, directionallightDepthLocation, 2);

    // Try to load and compile spotlight shaders
    GLuint fragspotlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "spotlight.frag");
    GLuint spotlightProgramObject = glCreateProgram();
    glAttachShader(spotlightProgramObject, vertBlitShaderId);
    glAttachShader(spotlightProgramObject, fragspotlightShaderId);
    glLinkProgram(spotlightProgramObject);
    if (check_link_error(spotlightProgramObject) < 0)
        exit(1);
    GLuint spotlightColorLocation = glGetUniformLocation(spotlightProgramObject, "ColorBuffer");
    GLuint spotlightNormalLocation = glGetUniformLocation(spotlightProgramObject, "NormalBuffer");
    GLuint spotlightDepthLocation = glGetUniformLocation(spotlightProgramObject, "DepthBuffer");
    GLuint spotInverseProjectionLocation = glGetUniformLocation(spotlightProgramObject, "InverseProjection");
    glProgramUniform1i(spotlightProgramObject, spotlightColorLocation, 0);
    glProgramUniform1i(spotlightProgramObject, spotlightNormalLocation, 1);
    glProgramUniform1i(spotlightProgramObject, spotlightDepthLocation, 2);

    // Try to load and compile gamma shaders
    GLuint fragGammalightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "gamma.frag");
    GLuint gammaProgramObject = glCreateProgram();
    glAttachShader(gammaProgramObject, vertBlitShaderId);
    glAttachShader(gammaProgramObject, fragGammalightShaderId);
    glLinkProgram(gammaProgramObject);
    if (check_link_error(gammaProgramObject) < 0)
        exit(1);
    GLuint gammaTextureLocation = glGetUniformLocation(gammaProgramObject, "Texture");
    glProgramUniform1i(gammaProgramObject, gammaTextureLocation, 0);
    GLuint gammaGammaLocation = glGetUniformLocation(gammaProgramObject, "Gamma");

    // Try to load and compile freichen shaders
    //GLuint fragfreichenlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "freichen.frag");
    GLuint fragfreichenlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "sobel.frag");
    GLuint freichenProgramObject = glCreateProgram();
    glAttachShader(freichenProgramObject, vertBlitShaderId);
    glAttachShader(freichenProgramObject, fragfreichenlightShaderId);
    glLinkProgram(freichenProgramObject);
    if (check_link_error(freichenProgramObject) < 0)
        exit(1);
    GLuint freichenTextureLocation = glGetUniformLocation(freichenProgramObject, "Texture");
    glProgramUniform1i(freichenProgramObject, freichenTextureLocation, 0);
    GLuint freichenFactorLocation = glGetUniformLocation(freichenProgramObject, "Factor");

    // Try to load and compile blur shaders
    GLuint fragblurlightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "blur.frag");
    GLuint blurProgramObject = glCreateProgram();
    glAttachShader(blurProgramObject, vertBlitShaderId);
    glAttachShader(blurProgramObject, fragblurlightShaderId);
    glLinkProgram(blurProgramObject);
    if (check_link_error(blurProgramObject) < 0)
        exit(1);
    GLuint blurTextureLocation = glGetUniformLocation(blurProgramObject, "Texture");
    glProgramUniform1i(blurProgramObject, blurTextureLocation, 0);
    GLuint blurSampleCountLocation = glGetUniformLocation(blurProgramObject, "SampleCount");
    GLuint blurDirectionLocation = glGetUniformLocation(blurProgramObject, "Direction");

    // Try to load and compile coc shaders
    GLuint fragcoclightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "coc.frag");
    GLuint cocProgramObject = glCreateProgram();
    glAttachShader(cocProgramObject, vertBlitShaderId);
    glAttachShader(cocProgramObject, fragcoclightShaderId);
    glLinkProgram(cocProgramObject);
    if (check_link_error(cocProgramObject) < 0)
        exit(1);
    GLuint cocTextureLocation = glGetUniformLocation(cocProgramObject, "Texture");
    glProgramUniform1i(cocProgramObject, cocTextureLocation, 0);
    GLuint cocScreenToViewCountLocation = glGetUniformLocation(cocProgramObject, "ScreenToView");
    GLuint cocFocusnLocation = glGetUniformLocation(cocProgramObject, "Focus");

    // Try to load and compile dof shaders
    GLuint fragdoflightShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "dof.frag");
    GLuint dofProgramObject = glCreateProgram();
    glAttachShader(dofProgramObject, vertBlitShaderId);
    glAttachShader(dofProgramObject, fragdoflightShaderId);
    glLinkProgram(dofProgramObject);
    if (check_link_error(dofProgramObject) < 0)
        exit(1);
    GLuint dofColorLocation = glGetUniformLocation(dofProgramObject, "Color");
    glProgramUniform1i(dofProgramObject, dofColorLocation, 0);
    GLuint dofCoCLocation = glGetUniformLocation(dofProgramObject, "CoC");
    glProgramUniform1i(dofProgramObject, dofCoCLocation, 1);
    GLuint dofBlurLocation = glGetUniformLocation(dofProgramObject, "Blur");
    glProgramUniform1i(dofProgramObject, dofBlurLocation, 2);

   
   // Try to load and compile objects shaders
    GLuint vertSceneShaderId = compile_shader_from_file(GL_VERTEX_SHADER, "trineGL.vert");
    GLuint fragSceneShaderId = compile_shader_from_file(GL_FRAGMENT_SHADER, "trineGL.frag");
    GLuint sceneProgramObject = glCreateProgram();
    glAttachShader(sceneProgramObject, vertSceneShaderId);
    glAttachShader(sceneProgramObject, fragSceneShaderId);
    glLinkProgram(sceneProgramObject);
    if (check_link_error(sceneProgramObject) < 0)
        exit(1);

    // Upload uniforms
    GLuint mvpLocation2 = glGetUniformLocation(sceneProgramObject, "MVP");
    GLuint mvLocation2 = glGetUniformLocation(sceneProgramObject, "MV");
    GLuint timeLocation2 = glGetUniformLocation(sceneProgramObject, "Time");
    GLuint diffuseLocation2 = glGetUniformLocation(sceneProgramObject, "Diffuse");
    GLuint diffuseColorLocation = glGetUniformLocation(sceneProgramObject, "DiffuseColor");
    GLuint specLocation2 = glGetUniformLocation(sceneProgramObject, "Specular");
    GLuint lightLocation2 = glGetUniformLocation(sceneProgramObject, "Light");
    GLuint specularPowerLocation2 = glGetUniformLocation(sceneProgramObject, "SpecularPower");
    GLuint instanceCountLocation2 = glGetUniformLocation(sceneProgramObject, "InstanceCount");
    glProgramUniform1i(sceneProgramObject, diffuseLocation2, 0);
    glProgramUniform1i(sceneProgramObject, specLocation2, 1);


   if (!checkError("Shaders"))
        exit(1);


    // Load the scene
    const aiScene* scene = NULL;

    std::string pathFile3D = "./scene_v4/scene_v4.obj";

    const char * charPathFile3D = pathFile3D.c_str();

    scene = aiImportFile(charPathFile3D, aiProcessPreset_TargetRealtime_MaxQuality);

    if (!scene) {
          fprintf(stderr, "Error: impossible to open the scene\n");
          exit( EXIT_FAILURE );
    }

    GLuint * assimp_vao = new GLuint[scene->mNumMeshes];
    glm::mat4 * assimp_objectToWorld = new glm::mat4[scene->mNumMeshes];
    glGenVertexArrays(scene->mNumMeshes, assimp_vao);
    GLuint * assimp_vbo = new GLuint[scene->mNumMeshes*4];
    glGenBuffers(scene->mNumMeshes*4, assimp_vbo);

    float * assimp_diffuse_colors = new float[scene->mNumMeshes*3];
    GLuint * assimp_diffuse_texture_ids = new GLuint[scene->mNumMeshes*3];


    for (unsigned int i =0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* m = scene->mMeshes[i];
        GLuint * faces = new GLuint[m->mNumFaces*3];
        for (unsigned int j = 0; j < m->mNumFaces; ++j)
        {
            const aiFace& f = m->mFaces[j];
            faces[j*3] = f.mIndices[0];
            faces[j*3+1] = f.mIndices[1];
            faces[j*3+2] = f.mIndices[2];
        }

      
        glBindVertexArray(assimp_vao[i]);
        // Bind indices and upload data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, assimp_vbo[i*4]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->mNumFaces*3 *sizeof(GLuint), faces, GL_STATIC_DRAW);

        // Bind vertices and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+1]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mVertices, GL_STATIC_DRAW);
        
        // Bind normals and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+2]);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mNormals, GL_STATIC_DRAW);
        
        // Bind uv coords and upload data
        glBindBuffer(GL_ARRAY_BUFFER, assimp_vbo[i*4+3]);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        if (m->HasTextureCoords(0))
            glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mTextureCoords[0], GL_STATIC_DRAW);
        else
            glBufferData(GL_ARRAY_BUFFER, m->mNumVertices*3*sizeof(float), m->mVertices, GL_STATIC_DRAW);
        delete[] faces;


        const aiMaterial * mat = scene->mMaterials[m->mMaterialIndex];
        int texIndex = 0;
        aiString texPath;


        if(AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, texIndex, &texPath))
        {
            std::string path = pathFile3D;
            size_t pos = path.find_last_of("\\/");
            std::string basePath = (std::string::npos == pos) ? "" : path.substr(0, pos + 1);
            std::string fileloc = basePath + texPath.data;
            pos = fileloc.find_last_of("\\");
            fileloc = (std::string::npos == pos) ? fileloc : fileloc.replace(pos, 1, "/");
            int x;
            int y;
            int comp;
            glGenTextures(1, assimp_diffuse_texture_ids + i);
            unsigned char * diffuse = stbi_load(fileloc.c_str(), &x, &y, &comp, 3);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, assimp_diffuse_texture_ids[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            assimp_diffuse_texture_ids[i] = 0;
        }


        aiColor4D diffuse;
        if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
        {
            assimp_diffuse_colors[i*3] = diffuse.r;
            assimp_diffuse_colors[i*3+1] = diffuse.g;
            assimp_diffuse_colors[i*3+2] = diffuse.b;
        }
        else
        {
            assimp_diffuse_colors[i*3] = 1.f;
            assimp_diffuse_colors[i*3+1] = 0.f;
            assimp_diffuse_colors[i*3+2] = 1.f;
        }
    }


    std::stack<aiNode*> stack;
    stack.push(scene->mRootNode);
    while(stack.size()>0)
    {
        aiNode * node = stack.top();
        for (unsigned int i =0; i < node->mNumMeshes; ++i)
        {
            aiMatrix4x4 t = node->mTransformation;
            assimp_objectToWorld[i] = glm::mat4(t.a1, t.a2, t.a3, t.a4,
                                                t.b1, t.b2, t.b3, t.b4,
                                                t.c1, t.c2, t.c3, t.c4,
                                                t.d1, t.d2, t.d3, t.d4);
        }
        stack.pop();
    }

    // // Unbind everything. Potentially illegal on some implementations
    // glBindVertexArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    

    // Load geometry
    //int cube_triangleCount = 12;
    int cube_triangleList[] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 19, 17, 20, 21, 22, 23, 24, 25, 26, };
    float cube_uvs[] = {0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,  1.f, 1.f,  0.f, 0.f, 0.f, 0.f, 1.f, 1.f,  1.f, 0.f,  };
    float cube_vertices[] = {-0.5, -0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5 };
    float cube_normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, };
    //int plane_triangleCount = 2;
    int plane_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float plane_uvs[] = {0.f, 0.f, 0.f, 50.f, 50.f, 0.f, 50.f, 50.f};
    float plane_vertices[] = {-50.0, -1.0, 50.0, 50.0, -1.0, 50.0, -50.0, -1.0, -50.0, 50.0, -1.0, -50.0};
    float plane_normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    int   quad_triangleCount = 2;
    int   quad_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float quad_vertices[] =  {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    // Vertex Array Object
    GLuint vao[3];
    glGenVertexArrays(3, vao);

    // Vertex Buffer Objects
    GLuint vbo[10];
    glGenBuffers(10, vbo);

    // Cube
    glBindVertexArray(vao[0]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_triangleList), cube_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_normals), cube_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[3]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_uvs), cube_uvs, GL_STATIC_DRAW);

    // Plane
    glBindVertexArray(vao[1]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[4]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[5]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[6]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[7]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    // Quad
    glBindVertexArray(vao[2]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[8]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_triangleList), quad_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[9]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Unbind everything. Potentially illegal on some implementations
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Update and bind uniform buffer object
    GLuint ubo[1];
    glGenBuffers(1, ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
    GLint uboSize = 0;
    glGetActiveUniformBlockiv(pointlightProgramObject, pointlightLightLocation, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
    glGetActiveUniformBlockiv(directionallightProgramObject, pointlightLightLocation, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);

    // Ignore ubo size, allocate it sufficiently big for all light data structures
    uboSize = 512;

    glBufferData(GL_UNIFORM_BUFFER, uboSize, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Init frame buffers
    GLuint gbufferFbo;
    GLuint gbufferTextures[3];
    GLuint gbufferDrawBuffers[2];
    glGenTextures(3, gbufferTextures);

    // Create color texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create Framebuffer Object
    glGenFramebuffers(1, &gbufferFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
    gbufferDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    gbufferDrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glDrawBuffers(2, gbufferDrawBuffers);

    // Attach textures to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, gbufferTextures[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 , GL_TEXTURE_2D, gbufferTextures[1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gbufferTextures[2], 0);

    // Create Fx Framebuffer Object
    GLuint fxFbo;
    GLuint fxDrawBuffers[1];
    glGenFramebuffers(1, &fxFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fxFbo);
    fxDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, fxDrawBuffers);

    // Create Fx textures
    const int FX_TEXTURE_COUNT = 4;
    GLuint fxTextures[FX_TEXTURE_COUNT];
    glGenTextures(FX_TEXTURE_COUNT, fxTextures);
    for (int i = 0; i < FX_TEXTURE_COUNT; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, fxTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Attach first fx texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[0], 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building framebuffer\n");
        exit( EXIT_FAILURE );
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    checkError("Framebuffers");

    camera_pan(camera, 3, 0);

    do
    {
        if (camera.o.x <17)
            camera_pan(camera, -0.001, 0);
        t = glfwGetTime() * speed;
        ImGui_ImplGlfwGL3_NewFrame();

        // Mouse states
        int leftButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT );
        int rightButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_RIGHT );
        int middleButton = glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_MIDDLE );

        if( leftButton == GLFW_PRESS )
            guiStates.turnLock = false;
        else
            guiStates.turnLock = false;

        if( rightButton == GLFW_PRESS )
            guiStates.zoomLock = false;
        else
            guiStates.zoomLock = false;

        if( middleButton == GLFW_PRESS )
            guiStates.panLock = false;
        else
            guiStates.panLock = false;

        // Camera movements
        int altPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT);
        if (!altPressed && (leftButton == GLFW_PRESS || rightButton == GLFW_PRESS || middleButton == GLFW_PRESS))
        {
            double x; double y;
            glfwGetCursorPos(window, &x, &y);
            guiStates.lockPositionX = x;
            guiStates.lockPositionY = y;
        }
        if (altPressed == GLFW_PRESS)
        {
            double mousex; double mousey;
            glfwGetCursorPos(window, &mousex, &mousey);
            int diffLockPositionX = mousex - guiStates.lockPositionX;
            int diffLockPositionY = mousey - guiStates.lockPositionY;
            if (guiStates.zoomLock)
            {
                float zoomDir = 0.0;
                if (diffLockPositionX > 0)
                    zoomDir = -1.f;
                else if (diffLockPositionX < 0 )
                    zoomDir = 1.f;
                camera_zoom(camera, zoomDir * GUIStates::MOUSE_ZOOM_SPEED);
            }
            else if (guiStates.turnLock)
            {
                camera_turn(camera, diffLockPositionY * GUIStates::MOUSE_TURN_SPEED,
                            diffLockPositionX * GUIStates::MOUSE_TURN_SPEED);

            }
            else if (guiStates.panLock)
            {
                camera_pan(camera, diffLockPositionX * GUIStates::MOUSE_PAN_SPEED,
                            diffLockPositionY * GUIStates::MOUSE_PAN_SPEED);
            }
            guiStates.lockPositionX = mousex;
            guiStates.lockPositionY = mousey;
        }

        // Default states
        glEnable(GL_DEPTH_TEST);

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Viewport 
        glViewport( 0, 0, width, height  );

        // Bind gbuffer
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);

        // Clear the gbuffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Get camera matrices
        glm::mat4 projection = glm::perspective(45.0f, widthf / heightf, 0.1f, 100.f); 
        glm::mat4 worldToView = glm::lookAt(camera.eye, camera.o, camera.up);
        glm::mat4 objectToWorld;
        glm::mat4 mv = worldToView * objectToWorld;
        glm::mat4 mvp = projection * mv;
        //glm::mat4 inverseProjection = glm::transpose(glm::inverse(projection));
        glm::mat4 inverseProjection = glm::inverse(projection);

        glm::vec4 light = worldToView * glm::vec4(10.0, 10.0, 0.0, 0.0);

        

        // Select shader
        glUseProgram(sceneProgramObject);

        // Upload uniforms
        glProgramUniformMatrix4fv(sceneProgramObject, mvpLocation2, 1, 0, glm::value_ptr(mvp));
        glProgramUniformMatrix4fv(sceneProgramObject, mvLocation2, 1, 0, glm::value_ptr(mv));
        glProgramUniform3fv(sceneProgramObject, lightLocation2, 1, glm::value_ptr(glm::vec3(light) / light.w));
        glProgramUniform1i(sceneProgramObject, instanceCountLocation2, (int) instanceCount);
        glProgramUniform1f(sceneProgramObject, specularPowerLocation2, 30.f);
        glProgramUniform1f(sceneProgramObject, timeLocation2, t);

        // Render vaos
        glActiveTexture(GL_TEXTURE0);
        glm::mat4 scale = glm::scale(glm::mat4(), glm::vec3(0.01));
        for (unsigned int i =0; i < scene->mNumMeshes; ++i)
        {
            GLuint subIndex = 1;
            if (assimp_diffuse_texture_ids[i] > 0) {
                glBindTexture(GL_TEXTURE_2D, assimp_diffuse_texture_ids[i]);
                subIndex = 0;
            }
            glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &subIndex);
            glm::mat4 mvScaled = worldToView * scale * assimp_objectToWorld[i];
            glm::mat4 mvpScaled = projection * mvScaled;
            glProgramUniformMatrix4fv(sceneProgramObject, mvpLocation2, 1, 0, glm::value_ptr(mvpScaled));
            glProgramUniformMatrix4fv(sceneProgramObject, mvLocation2, 1, 0, glm::value_ptr(mvScaled));
            glProgramUniform3fv(sceneProgramObject, diffuseColorLocation, 1, assimp_diffuse_colors + 3*i);
            const aiMesh* m = scene->mMeshes[i];
            glBindVertexArray(assimp_vao[i]);
            glDrawElements(GL_TRIANGLES, m->mNumFaces * 3, GL_UNSIGNED_INT, (void*)0);
        }



        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);


        // Select shader
        glUseProgram(gbufferProgramObject);

        // Upload uniforms
        glProgramUniformMatrix4fv(gbufferProgramObject, mvpLocation, 1, 0, glm::value_ptr(mvp));
        glProgramUniformMatrix4fv(gbufferProgramObject, mvLocation, 1, 0, glm::value_ptr(mv));
        glProgramUniform1i(gbufferProgramObject, instanceCountLocation, (int) instanceCount);
        glProgramUniform1f(gbufferProgramObject, specularPowerLocation, 30.f);
        glProgramUniform1f(gbufferProgramObject, timeLocation, t);
        glProgramUniform1i(gbufferProgramObject, diffuseLocation, 0);
        glProgramUniform1i(gbufferProgramObject, specLocation, 1);
        glProgramUniformMatrix4fv(pointlightProgramObject, pointInverseProjectionLocation, 1, 0, glm::value_ptr(inverseProjection));
        glProgramUniformMatrix4fv(directionallightProgramObject, directionalInverseProjectionLocation, 1, 0, glm::value_ptr(inverseProjection));
        glProgramUniformMatrix4fv(spotlightProgramObject, spotInverseProjectionLocation, 1, 0, glm::value_ptr(inverseProjection));

        // Render vaos
        glBindVertexArray(vao[0]);
        //glDrawElements(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        glProgramUniform1f(gbufferProgramObject, timeLocation, 0.f);
        glBindVertexArray(vao[1]);
        //glDrawElements(GL_TRIANGLES, plane_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        glBindFramebuffer(GL_FRAMEBUFFER, fxFbo);
        // Attach first fx texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[0], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // Select textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);

        // Bind the same VAO for all lights
        glBindVertexArray(vao[2]);

        // Render point lights
        glUseProgram(pointlightProgramObject);
        struct PointLight
        {
            glm::vec3 position;
            int padding;
            glm::vec3 color;
            float intensity;
        };
        for (int i = 0; i < pointLightCount; ++i)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            // PointLight p = { 
            //     glm::vec3( worldToView * glm::vec4((pointLightCount*cosf(t)) * sinf(t*i), 1.0, fabsf(pointLightCount*sinf(t)) * cosf(t*i), 1.0)),
            //     0,
            //     glm::vec3(fabsf(cos(t+i*2.f)), 1.-fabsf(sinf(t+i)) , 0.5f + 0.5f-fabsf(cosf(t+i)) ),
            //     0.5f + fabsf(cosf(t+i))
            // };
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(X,Y,Z,1)),
                0,
                glm::vec3(R,G,B),
                I
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 1
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(1.5,0.5,-4.8,1)),
                0,
                glm::vec3(0,0,1),
                1.5
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 2
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-10,2.681,-5.83,1)),
                0,
                glm::vec3(1,0.357,1),
                10
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 3
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-13.872,0.936,-3.319,1)),
                0,
                glm::vec3(1,0.46,0.009),
                2.085
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 4
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-10.818,0.085,-1.617,1)),
                0,
                glm::vec3(0,1,0.809),
                0.183
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 5
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-6.043,-1.957,0.426,1)),
                0,
                glm::vec3(1,0,1),
                2.213
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 6
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(0.596,3.149,-6.553,1)),
                0,
                glm::vec3(1,0.272,0.489),
                7.149
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 7
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(11.149,7.234,-5.702,1)),
                0,
                glm::vec3(1,0,0),
                10
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 8
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(8.085,-2.979,-1.617,1)),
                0,
                glm::vec3(0.268,1,1),
                3.957
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 9
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(12.34,0.426,-4.34,1)),
                0,
                glm::vec3(0.889,0.396,0),
                0.736
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 10
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(17.277,0.085,2.638,1)),
                0,
                glm::vec3(0.723,0.311,0.779),
                0.723
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Point light 11
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(17.277,3.489,-5.872,1)),
                0,
                glm::vec3(1,0.532,0.311),
                5.234
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 12
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(16.085,0.255,-7.915,1)),
                0,
                glm::vec3(0,0.532,0.604),
                9.894
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 13
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-14.553,1.277,-0.426,1)),
                0,
                glm::vec3(1,0.196,0),
                3.957
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 14
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-7.064,1.957,-0.426,1)),
                0,
                glm::vec3(1,0.536,0.745),
                4.213
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 15
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(-3.489,-0.255,0.596,1)),
                0,
                glm::vec3(1,0.174,0),
                10
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 16
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(4.34,4.851,0.596,1)),
                0,
                glm::vec3(1,1,1),
                10
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }


        // Point light 17
        {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
            PointLight p = { 
                glm::vec3( worldToView * glm::vec4(16.085,5.362,1.106,1)),
                0,
                glm::vec3(1,0,1),
                10
            };

            PointLight * pointLightBuffer = (PointLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *pointLightBuffer = p;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, pointlightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }



        // Render directional lights
        glUseProgram(directionallightProgramObject);
        struct DirectionalLight
        {
            glm::vec3 direction;
            int padding;
            glm::vec3 color;
            float intensity;
        };
        for (int i = 0; i < directionalLightCount; ++i)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
             DirectionalLight d = { 
                glm::vec3( worldToView * glm::vec4(1.0, -1.0, -1.0, 0.0)),
                0,
                glm::vec3(0.3, 0.3, 1.0),
                0.5f
            };
            DirectionalLight * directionalLightBuffer = (DirectionalLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
            *directionalLightBuffer = d;
            glUnmapBuffer(GL_UNIFORM_BUFFER);
            glBindBufferBase(GL_UNIFORM_BUFFER, directionallightLightLocation, ubo[0]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        // Render spot lights
        glUseProgram(spotlightProgramObject);
        struct SpotLight
        {
            glm::vec3 position;
            float angle;
            glm::vec3 direction;
            float penumbraAngle;
            glm::vec3 color;
            float intensity;
        };
        // for (int i = 0; i < spotLightCount; ++i)
        // {
        //     glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
        //     SpotLight s = { 
        //         glm::vec3( worldToView * glm::vec4((spotLightCount*sinf(t)) * cosf(t*i), 1.f + sinf(t * i), fabsf(spotLightCount*cosf(t)) * sinf(t*i), 1.0)),
        //         45.f + 20.f * cos(t + i),
        //         glm::vec3( worldToView * glm::vec4(sinf(t*10.0+i), -1.0, 0.0, 0.0)),
        //         60.f + 20.f * cos(t + i),
        //         glm::vec3(fabsf(cos(t+i*2.f)), 1.-fabsf(sinf(t+i)) , 0.5f + 0.5f-fabsf(cosf(t+i))),
        //         1.0
        //     };
        //     SpotLight * spotLightBuffer = (SpotLight *)glMapBufferRange(GL_UNIFORM_BUFFER, 0, uboSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        //     *spotLightBuffer = s;
        //     glUnmapBuffer(GL_UNIFORM_BUFFER);
        //     glBindBufferBase(GL_UNIFORM_BUFFER, spotlightLightLocation, ubo[0]);
        //     glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        // }        

        // End additive blending
        glDisable(GL_BLEND);

        // Attach fx texture #1 to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[1], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);

        // freichen
        glUseProgram(freichenProgramObject);
        glProgramUniform1f(freichenProgramObject, freichenFactorLocation, factor);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxTextures[0]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Attach fx texture #0 to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[2], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);

        // vertical blur
        glUseProgram(blurProgramObject);
        glProgramUniform1i(blurProgramObject, blurSampleCountLocation, (int) sampleCount);
        glProgramUniform2i(blurProgramObject, blurDirectionLocation, 0, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxTextures[1]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Attach fx texture #1 to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[0], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);
        // horizontal blur
        glProgramUniform2i(blurProgramObject, blurDirectionLocation, 1, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxTextures[2]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Attach fx texture #1 to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[2], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);
        // CoC compute
        glUseProgram(cocProgramObject);
        glProgramUniform3f(cocProgramObject, cocFocusnLocation, focusPlane, nearPlane, farPlane);
        glProgramUniformMatrix4fv(cocProgramObject, cocScreenToViewCountLocation, 1, 0, glm::value_ptr(inverseProjection));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Attach fx texture #1 to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fxTextures[3], 0);
        // Only the color buffer is used
        glClear(GL_COLOR_BUFFER_BIT);
        // dof compute
        glUseProgram(dofProgramObject);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxTextures[1]); // Color
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, fxTextures[2]); // CoC
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, fxTextures[0]); // Blur
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Write to back buffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Gamma
        glUseProgram(gammaProgramObject);
        glProgramUniform1f(gammaProgramObject, gammaGammaLocation, gamma);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxTextures[3]);
        glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Bind blit shader
        glUseProgram(blitProgramObject);
        // Upload uniforms
        // use only unit 0
        glActiveTexture(GL_TEXTURE0);
        // Bind VAO
        glBindVertexArray(vao[2]);

        // // Viewport 
        // glViewport( 0, 0, width/4, height/4  );
        // // Bind texture
        // glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
        // // Draw quad
        // glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        // // Viewport 
        // glViewport( width/4, 0, width/4, height/4  );
        // // Bind texture
        // glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
        // // Draw quad
        // glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        // // Viewport 
        // glViewport( width/4 * 2, 0, width/4, height/4  );
        // // Bind texture
        // glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
        // // Draw quad
        // glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        // // Viewport 
        // glViewport( width/4 * 3, 0, width/4, height/4  );
        // // Bind texture
        // glBindTexture(GL_TEXTURE_2D, fxTextures[(int)t%4]);
        // // Draw quad
        // glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        // Draw UI

        /*imguiSlider("Point Lights", &pointLightCount, 0.0, 100.0, 1);
        imguiSlider("Directional Lights", &directionalLightCount, 0.0, 100.0, 1);
        imguiSlider("Spot Lights", &spotLightCount, 0.0, 100.0, 1);
        imguiSlider("Blur Samples", &sampleCount, 3.0, 65.0, 2.0);
*/

        // ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        // ImGui::Begin("aogl");
        // ImGui::SliderFloat("X", &X, -20.0f, 20.0f);
        // ImGui::SliderFloat("Y", &Y, -20.0f, 20.0f);
        // ImGui::SliderFloat("Z", &Z, -20.0f, 20.0f);
        // ImGui::SliderFloat("R", &R, 0.0f, 1.0f);
        // ImGui::SliderFloat("G", &G, 0.0f, 1.0f);
        // ImGui::SliderFloat("B", &B, 0.0f, 1.0f);
        // ImGui::SliderFloat("I", &I, 0.0f, 10.0f);
        // ImGui::SliderFloat("Speed", &speed, 0.0f, 1.0f);
        // ImGui::SliderFloat("Gamma", &gamma, 0.01f, 3.0f);
        // ImGui::SliderFloat("Factor", &factor, 0.01f, 5.0f);
        // ImGui::SliderFloat("Focus plane", &focusPlane, 1.f, 100.f);
        // ImGui::SliderFloat("Near plane", &nearPlane, 1.f, 100.f);
        // ImGui::SliderFloat("Far plane", &farPlane, 1.f, 100.f);
        // ImGui::DragInt("Sample Count", &sampleCount, .1f, 0, 100);
        // ImGui::DragInt("Point Lights", &pointLightCount, .1f, 0, 100);
        // ImGui::DragInt("Directional Lights", &directionalLightCount, .1f, 0, 100);
        // ImGui::DragInt("Spot Lights", &spotLightCount, .1f, 0, 100);
        // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        // ImGui::End();

        // ImGui::Render();
        // Check for errors
        checkError("End loop");

        glfwSwapBuffers(window);
        glfwPollEvents();
    } // Check if the ESC key was pressed
    while( glfwGetKey( window, GLFW_KEY_ESCAPE ) != GLFW_PRESS );

    // Close OpenGL window and terminate GLFW
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    exit( EXIT_SUCCESS );
}

// No windows implementation of strsep
char * strsep_custom(char **stringp, const char *delim)
{
    register char *s;
    register const char *spanp;
    register int c, sc;
    char *tok;
    if ((s = *stringp) == NULL)
        return (NULL);
    for (tok = s; ; ) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return (tok);
            }
        } while (sc != 0);
    }
    return 0;
}

int check_compile_error(GLuint shader, const char ** sourceBuffer)
{
    // Get error log size and print it eventually
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        char *token, *string;
        string = strdup(sourceBuffer[0]);
        int lc = 0;
        while ((token = strsep_custom(&string, "\n")) != NULL) {
           printf("%3d : %s\n", lc, token);
           ++lc;
        }
        fprintf(stderr, "Compile : %s", log);
        delete[] log;
    }
    // If an error happend quit
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
        return -1;     
    return 0;
}

int check_link_error(GLuint program)
{
    // Get link error log size and print it eventually
    int logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char * log = new char[logLength];
        glGetProgramInfoLog(program, logLength, &logLength, log);
        fprintf(stderr, "Link : %s \n", log);
        delete[] log;
    }
    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);        
    if (status == GL_FALSE)
        return -1;
    return 0;
}

GLuint compile_shader(GLenum shaderType, const char * sourceBuffer, int bufferSize)
{
    GLuint shaderObject = glCreateShader(shaderType);
    const char * sc[1] = { sourceBuffer };
    glShaderSource(shaderObject, 
                   1, 
                   sc,
                   NULL);
    glCompileShader(shaderObject);
    check_compile_error(shaderObject, sc);
    return shaderObject;
}

GLuint compile_shader_from_file(GLenum shaderType, const char * path)
{
    FILE * shaderFileDesc = fopen( path, "rb" );
    if (!shaderFileDesc)
        return 0;
    fseek ( shaderFileDesc , 0 , SEEK_END );
    long fileSize = ftell ( shaderFileDesc );
    rewind ( shaderFileDesc );
    char * buffer = new char[fileSize + 1];
    fread( buffer, 1, fileSize, shaderFileDesc );
    buffer[fileSize] = '\0';
    GLuint shaderObject = compile_shader(shaderType, buffer, fileSize );
    delete[] buffer;
    return shaderObject;
}


bool checkError(const char* title)
{
    int error;
    if((error = glGetError()) != GL_NO_ERROR)
    {
        std::string errorString;
        switch(error)
        {
        case GL_INVALID_ENUM:
            errorString = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            errorString = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            errorString = "GL_INVALID_OPERATION";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errorString = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            errorString = "GL_OUT_OF_MEMORY";
            break;
        default:
            errorString = "UNKNOWN";
            break;
        }
        fprintf(stdout, "OpenGL Error(%s): %s\n", errorString.c_str(), title);
    }
    return error == GL_NO_ERROR;
}

void camera_compute(Camera & c)
{
    c.eye.x = cos(c.theta) * sin(c.phi) * c.radius + c.o.x;   
    c.eye.y = cos(c.phi) * c.radius + c.o.y ;
    c.eye.z = sin(c.theta) * sin(c.phi) * c.radius + c.o.z;   
    c.up = glm::vec3(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
}

void camera_defaults(Camera & c)
{
    c.phi = 3.14/2.f;
    c.theta = 3.14/2.f;
    c.radius = 3.f;
    camera_compute(c);
}

void camera_zoom(Camera & c, float factor)
{
    c.radius += factor * c.radius ;
    if (c.radius < 0.1)
    {
        c.radius = 10.f;
        c.o = c.eye + glm::normalize(c.o - c.eye) * c.radius;
    }
    camera_compute(c);
}

void camera_turn(Camera & c, float phi, float theta)
{
    c.theta += 1.f * theta;
    c.phi   -= 1.f * phi;
    if (c.phi >= (2 * M_PI) - 0.1 )
        c.phi = 0.00001;
    else if (c.phi <= 0 )
        c.phi = 2 * M_PI - 0.1;
    camera_compute(c);
}

void camera_pan(Camera & c, float x, float y)
{
    glm::vec3 up(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
    glm::vec3 fwd = glm::normalize(c.o - c.eye);
    glm::vec3 side = glm::normalize(glm::cross(fwd, up));
    c.up = glm::normalize(glm::cross(side, fwd));
    // c.o[0] += up[0] * y * c.radius * 2;
    // c.o[1] += up[1] * y * c.radius * 2;
    // c.o[2] += up[2] * y * c.radius * 2;
    c.o[0] -= side[0] * x * c.radius * 2;
    c.o[1] -= side[1] * x * c.radius * 2;
    c.o[2] -= side[2] * x * c.radius * 2;       
    camera_compute(c);
}

void init_gui_states(GUIStates & guiStates)
{
    guiStates.panLock = false;
    guiStates.turnLock = false;
    guiStates.zoomLock = false;
    guiStates.lockPositionX = 0;
    guiStates.lockPositionY = 0;
    guiStates.camera = 0;
    guiStates.time = 0.0;
    guiStates.playing = false;
}

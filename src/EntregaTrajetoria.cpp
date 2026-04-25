#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// GLAD
#include <glad/glad.h>
// GLFW
#include <GLFW/glfw3.h>
// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Biblioteca para carregar imagens
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;

// Constantes da janela
const GLuint WIDTH = 800, HEIGHT = 600;

// =========================================================================
// VARIÁVEIS GLOBAIS DA CÂMERA
// =========================================================================
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

bool firstMouse = true;
float yaw   = -90.0f; 
float pitch =  0.0f;
float lastX =  WIDTH / 2.0f;
float lastY =  HEIGHT / 2.0f;
float fov   =  45.0f; 

// Timing
float deltaTime = 0.0f;	
float lastFrame = 0.0f;

// =========================================================================
// VARIÁVEIS DA TRAJETÓRIA DO OBJETO
// =========================================================================
vector<glm::vec3> trajectoryPoints; // Guarda os pontos de controle
glm::vec3 objectPos = glm::vec3(0.0f, 0.0f, 0.0f); // Posição atual do objeto

int currentPointIndex = 0; // Índice do ponto de origem atual
float t = 0.0f;            // Parâmetro 't' da interpolação (0.0 a 1.0)
float objectSpeed = 0.5f;  // Velocidade de deslocamento do objeto

// Controle para evitar múltiplos registros com um único clique do teclado
bool pKeyPressed = false; 

// =========================================================================
// SHADERS E FUNÇÕES AUXILIARES (Omiti a string completa para economizar espaço, 
// USE AS MESMAS STRINGS E FUNÇÕES DO ARQUIVO ANTERIOR)
// =========================================================================
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texCoord;
    out vec2 TexCoord;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * view * model * vec4(position, 1.0);
        TexCoord = texCoord;
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    in vec2 TexCoord;
    out vec4 color;
    uniform sampler2D ourTexture;
    void main() {
        color = texture(ourTexture, TexCoord);
    }
)glsl";

string getDirectoryPath(const string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    return (std::string::npos == pos) ? "" : filePath.substr(0, pos + 1);
}

int loadSimpleOBJ(string filePATH, int &nVertices, string &texturePath) {
    // ... [COLE AQUI A MESMA IMPLEMENTAÇÃO DO loadSimpleOBJ DO ARQUIVO ANTERIOR] ...
    // Para brevidade, mantenha a função idêntica à que você já possui.
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<GLfloat> vBuffer;

    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) return -1;
    string basePath = getDirectoryPath(filePATH);
    std::string line;

    while (std::getline(arqEntrada, line)) {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;
        if (word == "mtllib") {
            string mtlFile; ssline >> mtlFile;
            string mtlPath = basePath + mtlFile;
            std::ifstream mtlEntrada(mtlPath.c_str());
            if (mtlEntrada.is_open()) {
                string mtlLine;
                while (std::getline(mtlEntrada, mtlLine)) {
                    std::istringstream mtlSS(mtlLine);
                    string mtlWord; mtlSS >> mtlWord;
                    if (mtlWord == "map_Kd") {
                        mtlSS >> texturePath;
                        texturePath = basePath + texturePath; 
                        break; 
                    }
                }
                mtlEntrada.close();
            }
        } else if (word == "v") {
            glm::vec3 vertice; ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        } else if (word == "vt") {
            glm::vec2 vt; ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        } else if (word == "f") {
            while (ssline >> word) {
                int vi = 0, ti = 0;
                std::istringstream ss(word); std::string index;
                if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index, '/')) ti = !index.empty() ? std::stoi(index) - 1 : 0;

                vBuffer.push_back(vertices[vi].x); vBuffer.push_back(vertices[vi].y); vBuffer.push_back(vertices[vi].z);
                if (!texCoords.empty()) {
                    vBuffer.push_back(texCoords[ti].s); vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f);
                }
            }
        }
    }
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO); glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat))); glEnableVertexAttribArray(1);
    nVertices = vBuffer.size() / 5;  
    return VAO;
}

// Processa movimentação via teclado (WASD) e captura de pontos (P)
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Câmera Movimentação
    float cameraSpeed = 5.0f * deltaTime; 
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    // Registrar novo ponto de controle na posição atual da câmera ao apertar 'P'
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) { // Evita registrar múltiplos pontos segurando a tecla
            trajectoryPoints.push_back(cameraPos);
            cout << "Ponto adicionado [" << trajectoryPoints.size() << "]: (" 
                 << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << endl;
            pKeyPressed = true;
        }
    } else {
        pKeyPressed = false;
    }
}

// Processa rotação via mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Invertido
    lastX = xpos; lastY = ypos;
    xoffset *= 0.1f; yoffset *= 0.1f;
    yaw += xoffset; pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Processa Zoom via Scroll
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

// =========================================================================
// FUNÇÃO MAIN
// =========================================================================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Trajetoria Linear do Objeto", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    // Compila Shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); glCompileShader(vertexShader);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL); glCompileShader(fragmentShader);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); glAttachShader(shaderProgram, fragmentShader); glLinkProgram(shaderProgram);

    // Carrega Modelo
    int nVertices;
    string texturePath;
    GLuint objVAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, texturePath);

    // Carrega Textura
    GLuint textureID = 0;
    if (!texturePath.empty()) {
        glGenTextures(1, &textureID); glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_set_flip_vertically_on_load(true);
        int width, height, nrChannels;
        unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data); glGenerateMipmap(GL_TEXTURE_2D);
        }
        stbi_image_free(data);
    }

    // Loop Principal
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime(); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
        processInput(window);

        // =========================================================================
        // LÓGICA DE INTERPOLAÇÃO DA TRAJETÓRIA
        // =========================================================================
        if (trajectoryPoints.size() >= 2) {
            // Interpolação linear entre P0 e P1 [cite: 360]
            glm::vec3 p0 = trajectoryPoints[currentPointIndex];
            
            // O operador % garante que ao chegar no último ponto, o próximo seja o índice 0 (ciclo)
            int nextIndex = (currentPointIndex + 1) % trajectoryPoints.size(); 
            glm::vec3 p1 = trajectoryPoints[nextIndex];

            // Incrementa o parâmetro 't' baseado no tempo
            t += objectSpeed * deltaTime;

            // Se 't' chegar a 1.0, o objeto alcançou o ponto destino
            if (t >= 1.0f) {
                t = 0.0f; // Reseta o t
                currentPointIndex = nextIndex; // Atualiza o ponto de origem para ser o atual
            }

            // A função glm::mix faz exatamente P(t) = P0 + t*(P1 - P0)
            objectPos = glm::mix(p0, p1, t);
        } 
        else if (trajectoryPoints.size() == 1) {
            // Se tiver apenas 1 ponto, o objeto fica fixo nele
            objectPos = trajectoryPoints[0];
        }

        // Renderização Padrão
        glClearColor(0.0f, 0.5f, 1.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Aplica a posição calculada da trajetória na Matriz do Modelo
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, objectPos);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        if (textureID != 0) glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(objVAO);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);
        glBindVertexArray(0);

        glfwSwapBuffers(window); glfwPollEvents();
    }
    glDeleteVertexArrays(1, &objVAO); glfwTerminate(); return 0;
}
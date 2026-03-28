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

const GLuint WIDTH = 800, HEIGHT = 600;

// =========================================================================
// ESTRUTURA DE MATERIAL (Para armazenar os dados do .mtl)
// =========================================================================
struct Material {
    glm::vec3 Ka = glm::vec3(0.2f); // Ambiente (fallback)
    glm::vec3 Kd = glm::vec3(0.8f); // Difusa (fallback)
    glm::vec3 Ks = glm::vec3(1.0f); // Especular (fallback)
    float Ns = 32.0f;               // Shininess / Brilho
    string texturePath = "";
};

// =========================================================================
// SHADERS ATUALIZADOS COM MODELO DE PHONG
// =========================================================================
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texCoord;
    layout (location = 2) in vec3 normal; // NOVO: Vetor Normal lido do OBJ

    out vec3 FragPos;   // Posição do fragmento no mundo 3D
    out vec3 Normal;    // Vetor normal repassado
    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        // Calcula a posição do vértice no mundo real (precisamos para a luz difusa/especular bater certo)
        FragPos = vec3(model * vec4(position, 1.0));
        
        // Ajusta a normal matematicamente caso o objeto seja rotacionado ou escalonado
        Normal = mat3(transpose(inverse(model))) * normal;  
        TexCoord = texCoord;
        
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 color;

    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoord;

    uniform sampler2D ourTexture;
    
    // Variáveis de Iluminação
    uniform vec3 lightPos;   // Posição da lâmpada
    uniform vec3 viewPos;    // Posição da câmera (olho)
    uniform vec3 lightColor; // Cor da luz

    // Propriedades do Material vindas do arquivo .MTL
    uniform vec3 materialKa; // Ambiente
    uniform vec3 materialKd; // Difusa
    uniform vec3 materialKs; // Especular
    uniform float materialNs; // Brilho

    void main() {
        // 1. Componente AMBIENTE (Luz rebatida no cenário)
        vec3 ambient = lightColor * materialKa;
        
        // 2. Componente DIFUSA (Luz direta batendo no objeto)
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = lightColor * (diff * materialKd);
        
        // 3. Componente ESPECULAR (Brilho metálico/plástico)
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialNs);
        vec3 specular = lightColor * (spec * materialKs);  
        
        // RESULTADO FINAL: Soma as 3 luzes e multiplica pela cor da Textura
        vec3 result = ambient + diffuse + specular;
        vec4 texColor = texture(ourTexture, TexCoord);
        
        color = vec4(result, 1.0) * texColor;
    }
)glsl";

// =========================================================================
// FUNÇÕES AUXILIARES
// =========================================================================
string getDirectoryPath(const string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    return (std::string::npos == pos) ? "" : filePath.substr(0, pos + 1);
}

// =========================================================================
// CARREGADOR DE OBJ (Lê Normais e Coeficientes de Luz)
// =========================================================================
int loadSimpleOBJ(string filePATH, int &nVertices, Material &mat)
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<GLfloat> vBuffer;

    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) {
        std::cerr << "Erro ao tentar ler o arquivo " << filePATH << std::endl;
        return -1;
    }

    string basePath = getDirectoryPath(filePATH);
    std::string line;

    while (std::getline(arqEntrada, line)) {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;

        if (word == "mtllib") {
            string mtlFile;
            ssline >> mtlFile;
            string mtlPath = basePath + mtlFile;
            std::ifstream mtlEntrada(mtlPath.c_str());
            
            if (mtlEntrada.is_open()) {
                string mtlLine;
                while (std::getline(mtlEntrada, mtlLine)) {
                    std::istringstream mtlSS(mtlLine);
                    string mtlWord;
                    mtlSS >> mtlWord;
                    
                    // Extrai as componentes de Iluminação do Material
                    if (mtlWord == "Ka") mtlSS >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
                    if (mtlWord == "Kd") mtlSS >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
                    if (mtlWord == "Ks") mtlSS >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
                    if (mtlWord == "Ns") mtlSS >> mat.Ns;
                    
                    // Textura
                    if (mtlWord == "map_Kd") {
                        string texName;
                        mtlSS >> texName;
                        mat.texturePath = basePath + texName; 
                    }
                }
                mtlEntrada.close();
            }
        }
        else if (word == "v") {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        } 
        else if (word == "vt") {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        } 
        else if (word == "vn") {
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } 
        else if (word == "f") {
            while (ssline >> word) {
                int vi = 0, ti = 0, ni = 0;
                std::istringstream ss(word);
                std::string index;

                if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index, '/')) ti = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index)) ni = !index.empty() ? std::stoi(index) - 1 : 0;

                // 1. Posição
                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);
                
                // 2. Coordenadas de Textura
                if (!texCoords.empty()) {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f);
                }
                
                // 3. Normais (NOVO)
                if (!normals.empty()) {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); vBuffer.push_back(1.0f);
                }
            }
        }
    }
    arqEntrada.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Transfere o buffer para a GPU
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // AGORA TEMOS 8 VALORES POR VÉRTICE: (x,y,z, s,t, nx,ny,nz) -> Stride = 8 * sizeof(GLfloat)
    
    // Atributo 0: Posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: Textura (s, t) - Offset de 3
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    
    // Atributo 2: Normal (nx, ny, nz) - Offset de 5
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Atualiza a conta: VBuffer total dividido por 8 valores por vértice
    nVertices = vBuffer.size() / 8;  
    return VAO;
}

// =========================================================================
// MAIN
// =========================================================================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Leitor OBJ com Phong", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Falha ao criar janela" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // CARREGA O MODELO (Passando nossa struct Material)
    int nVertices;
    Material mat;
    GLuint objVAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, mat);

    if (objVAO == -1) {
        std::cout << "Abortando: Modelo não encontrado." << std::endl;
        return -1;
    }

    // CARREGA A TEXTURA
    GLuint textureID = 0;
    if (!mat.texturePath.empty()) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_set_flip_vertically_on_load(true);
        int width, height, nrChannels;
        unsigned char *data = stbi_load(mat.texturePath.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            std::cout << "Aviso: Falha ao carregar textura: " << mat.texturePath << std::endl;
        }
        stbi_image_free(data);
    }

    // Configurações de Câmera e Luz
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, -8.0f); // Posição da câmera
    glm::mat4 view = glm::translate(glm::mat4(1.0f), cameraPos);
    
    glm::vec3 lightPos = glm::vec3(2.0f, 3.0f, 2.0f);   // Lâmpada no alto, à direita e para frente
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // Luz branca pura

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fundo escuro para a luz ter mais destaque
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // 1. Envia as matrizes
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // 2. Envia os dados de iluminação
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(abs(cameraPos))); 
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));

        // 3. Envia os dados do material (.mtl)
        glUniform3fv(glGetUniformLocation(shaderProgram, "materialKa"), 1, glm::value_ptr(mat.Ka));
        glUniform3fv(glGetUniformLocation(shaderProgram, "materialKd"), 1, glm::value_ptr(mat.Kd));
        glUniform3fv(glGetUniformLocation(shaderProgram, "materialKs"), 1, glm::value_ptr(mat.Ks));
        glUniform1f(glGetUniformLocation(shaderProgram, "materialNs"), mat.Ns);

        // Desenha
        if (textureID != 0) glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(objVAO);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &objVAO);
    glfwTerminate();
    return 0;
}
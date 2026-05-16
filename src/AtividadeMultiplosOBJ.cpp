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
// CÓDIGO DOS SHADERS
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
    uniform vec3 highlightColor; // Para destacar o objeto selecionado

    void main() {
        vec4 texColor = texture(ourTexture, TexCoord);
        // Mistura a cor da textura com a cor de destaque (branco neutro por padrão)
        color = texColor * vec4(highlightColor, 1.0);
    }
)glsl";

// =========================================================================
// FUNÇÕES AUXILIARES DE CARREGAMENTO (Mantidas do original)
// =========================================================================
string getDirectoryPath(const string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    return (std::string::npos == pos) ? "" : filePath.substr(0, pos + 1);
}

int loadSimpleOBJ(string filePATH, int &nVertices, string &texturePath)
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
                    if (mtlWord == "map_Kd") {
                        mtlSS >> texturePath;
                        texturePath = basePath + texturePath; 
                        break; 
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

                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);
                
                if (!texCoords.empty()) {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }
            }
        }
    }
    arqEntrada.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Textura (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 5;  
    return VAO;
}

// =========================================================================
// ESTRUTURA DO OBJETO 3D
// =========================================================================
struct Objeto3D {
    GLuint VAO = 0;
    int nVertices = 0;
    GLuint textureID = 0;
    
    // Transformações
    glm::vec3 posicao = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotacao = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 escala = glm::vec3(1.0f, 1.0f, 1.0f);

    // Método para carregar a geometria e a textura do objeto
    void carregar(std::string caminhoArquivo, glm::vec3 posInicial) {
        posicao = posInicial;
        std::string texturePath;
        
        VAO = loadSimpleOBJ(caminhoArquivo, nVertices, texturePath);

        if (VAO == -1) {
            std::cout << "Erro ao carregar modelo: " << caminhoArquivo << std::endl;
            return;
        }

        if (!texturePath.empty()) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            stbi_set_flip_vertically_on_load(true);
            
            int width, height, nrChannels;
            unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
            } else {
                std::cout << "Falha ao carregar a textura: " << texturePath << std::endl;
            }
            stbi_image_free(data);
        }
    }
};

// =========================================================================
// FUNÇÃO MAIN
// =========================================================================
int main()
{
    // 1. Inicializa GLFW e Janela
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Atividade Múltiplos OBJs", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Falha ao criar janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 2. Inicializa GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Falha ao inicializar GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // 3. Compila os Shaders
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

    // =========================================================================
    // 4. INICIALIZAÇÃO DA CENA (MÚLTIPLOS OBJETOS)
    // =========================================================================
    std::vector<Objeto3D> cena;

    // Criando e carregando os objetos na cena
    // Nota: O método .carregar() deve ser chamado apenas após inicializar o GLAD!
    Objeto3D obj1, obj2;
    obj1.carregar("../assets/Modelos3D/Suzanne.obj", glm::vec3(-2.5f, 0.0f, 0.0f));
    obj2.carregar("../assets/Modelos3D/Suzanne.obj", glm::vec3( 2.5f, 0.0f, 0.0f));

    cena.push_back(obj1);
    cena.push_back(obj2);

    // Variáveis de controle de interação
    int objSelecionado = 0;
    int modoTransformacao = 0; // 0 = Translação, 1 = Rotação, 2 = Escala
    bool tabPressionado = false;

    // 5. Configura Matrizes de Câmera (View) e Projeção
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -8.0f));

    // =========================================================================
    // LOOP PRINCIPAL
    // =========================================================================
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Limpa a tela com fundo azul claro
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- CONTROLES DE INTERAÇÃO ---
        
        // Seleção do objeto com TAB (Evita múltiplas trocas se segurar o botão)
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressionado) {
                objSelecionado = (objSelecionado + 1) % cena.size();
                tabPressionado = true;
                std::cout << "Objeto selecionado: " << objSelecionado << std::endl;
            }
        } else {
            tabPressionado = false;
        }

        // Alterar o Modo de Transformação
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) modoTransformacao = 0; // Translação
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) modoTransformacao = 1; // Rotação
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) modoTransformacao = 2; // Escala

        // Aplicar as transformações no objeto atualmente selecionado usando WASD
        Objeto3D& obj = cena[objSelecionado];
        float vel = 0.05f;

        if (modoTransformacao == 0) { // TRANSLAÇÃO
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.posicao.y += vel;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.posicao.y -= vel;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.posicao.x += vel;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.posicao.x -= vel;
        } 
        else if (modoTransformacao == 1) { // ROTAÇÃO (Exemplo nos eixos X e Y)
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.rotacao.x -= vel;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.rotacao.x += vel;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.rotacao.y += vel;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.rotacao.y -= vel;
        }
        else if (modoTransformacao == 2) { // ESCALA UNIFORME
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.escala += glm::vec3(vel);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.escala -= glm::vec3(vel);
        }

        // --- RENDERIZAÇÃO ---
        glUseProgram(shaderProgram);

        // Envia as matrizes da Câmera para o Shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Desenha todos os objetos da cena
        for (int i = 0; i < cena.size(); i++) {
            
            // Calcula a matriz Model: Translação -> Rotação -> Escala
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cena[i].posicao);
            model = glm::rotate(model, cena[i].rotacao.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, cena[i].escala);

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            // Destaca o objeto selecionado mudando a cor dele para amarelo (1.0, 1.0, 0.0)
            GLuint locColor = glGetUniformLocation(shaderProgram, "highlightColor");
            if (i == objSelecionado) {
                glUniform3f(locColor, 1.0f, 1.0f, 0.0f); 
            } else {
                glUniform3f(locColor, 1.0f, 1.0f, 1.0f); // Cor normal
            }

            // Ativa a textura do objeto específico
            if (cena[i].textureID != 0) {
                glBindTexture(GL_TEXTURE_2D, cena[i].textureID);
            }

            // Conecta o VAO do objeto e desenha
            glBindVertexArray(cena[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, cena[i].nVertices);
        }
        
        // Desconecta o VAO por segurança
        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    // Limpeza
    for (int i = 0; i < cena.size(); i++) {
        glDeleteVertexArrays(1, &cena[i].VAO);
    }
    
    glfwTerminate();
    return 0;
}
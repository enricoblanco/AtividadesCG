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
// CÓDIGO DOS SHADERS (ATUALIZADOS PARA ILUMINAÇÃO DE 3 PONTOS)
// =========================================================================
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texCoord;
    layout (location = 2) in vec3 normal; // Entrada das Normais para iluminação

    out vec2 TexCoord;
    out vec3 Normal;
    out vec3 FragPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(position, 1.0);
        FragPos = vec3(model * vec4(position, 1.0));
        // Ajusta as normais com base nas transformações do modelo (matriz normal)
        Normal = mat3(transpose(inverse(model))) * normal;
        TexCoord = texCoord;
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    in vec2 TexCoord;
    in vec3 Normal;
    in vec3 FragPos;

    out vec4 color;

    uniform sampler2D ourTexture;
    uniform vec3 highlightColor;
    uniform vec3 viewPos; // Posição da câmera para cálculo especular

    struct PointLight {
        vec3 position;
        vec3 color;
        float constant;
        float linear;
        float quadratic;
        bool enabled;
    };

    #define NR_LIGHTS 3
    uniform PointLight pointLights[NR_LIGHTS];

    // Função para calcular a contribuição de cada luz pontual
    vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor) {
        if(!light.enabled) return vec3(0.0);

        vec3 lightDir = normalize(light.position - fragPos);
        float distance = length(light.position - fragPos);
        
        // Fator de Atenuação baseado na distância
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        
        // Parcela Ambiente
        vec3 ambient = 0.1 * light.color * texColor;
        
        // Parcela Difusa (Atenuação aplicada especificamente aqui conforme enunciado)
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = light.color * diff * texColor * attenuation;
        
        // Parcela Especular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = light.color * spec * vec3(1.0) * attenuation;
        
        return (ambient + diffuse + specular);
    }

    void main() {
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);
        
        vec4 texColorObj = texture(ourTexture, TexCoord);
        vec3 texColor = texColorObj.rgb * highlightColor;

        vec3 result = vec3(0.0);
        
        // Acumula o efeito das 3 fontes de luz
        for(int i = 0; i < NR_LIGHTS; i++) {
            result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
        }

        color = vec4(result, texColorObj.a);
    }
)glsl";

// =========================================================================
// FUNÇÃO AUXILIAR DE CARREGAMENTO OBJ (SUPORTE A NORMAIS - STRIDE 8)
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

                // Posição (x, y, z)
                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);
                
                // Textura (s, t)
                if (!texCoords.empty()) {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f);
                }

                // Normal (nx, ny, nz)
                if (!normals.empty()) {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                } else {
                    vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); vBuffer.push_back(0.0f);
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
    
    // Posição (Atributo 0, tamanho 3, stride 8)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Textura (Atributo 1, tamanho 2, stride 8)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Normal (Atributo 2, tamanho 3, stride 8)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 8;  
    return VAO;
}

struct Objeto3D {
    GLuint VAO = 0;
    int nVertices = 0;
    GLuint textureID = 0;
    
    glm::vec3 posicao = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 rotacao = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 escala = glm::vec3(1.0f, 1.0f, 1.0f);

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

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Atividade Iluminacao 3 Pontos", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Falha ao criar janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Falha ao inicializar GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Compilação dos Shaders
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

    // Inicialização da cena
    std::vector<Objeto3D> cena;
    Objeto3D objPrincipal, objSecundario;
    
    // Carrega o objeto principal centralizado
    objPrincipal.carregar("../assets/Modelos3D/Suzanne.obj", glm::vec3(0.0f, 0.0f, 0.0f));
    objSecundario.carregar("../assets/Modelos3D/Suzanne.obj", glm::vec3(3.0f, 0.0f, -2.0f));

    cena.push_back(objPrincipal);
    cena.push_back(objSecundario);

    int objSelecionado = 0;
    int modoTransformacao = 0; 
    bool tabPressionado = false;

    // Estados de ativação das luzes (Key, Fill, Back)
    bool lightStates[3] = {true, true, true};
    bool key4Pressed = false, key5Pressed = false, key6Pressed = false;

    // Configuração de Câmera fixa para observação
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 8.0f);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), -cameraPos);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // Fundo escuro para destacar o efeito da iluminação
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- SELEÇÃO E TRANSFORMAÇÕES DOS OBJETOS ---
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressionado) {
                objSelecionado = (objSelecionado + 1) % cena.size();
                tabPressionado = true;
            }
        } else {
            tabPressionado = false;
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) modoTransformacao = 0;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) modoTransformacao = 1;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) modoTransformacao = 2;

        Objeto3D& obj = cena[objSelecionado];
        float vel = 0.05f;

        if (modoTransformacao == 0) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.posicao.y += vel;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.posicao.y -= vel;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.posicao.x += vel;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.posicao.x -= vel;
        } else if (modoTransformacao == 1) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.rotacao.x -= vel;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.rotacao.x += vel;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.rotacao.y += vel;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.rotacao.y -= vel;
        } else if (modoTransformacao == 2) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.escala += glm::vec3(vel);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.escala -= glm::vec3(vel);
        }

        // --- CONTROLES DE INTERRUPTORES DAS LUZES (Teclas 4, 5 e 6) ---
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) { if (!key4Pressed) { lightStates[0] = !lightStates[0]; key4Pressed = true; } } else key4Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) { if (!key5Pressed) { lightStates[1] = !lightStates[1]; key5Pressed = true; } } else key5Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) { if (!key6Pressed) { lightStates[2] = !lightStates[2]; key6Pressed = true; } } else key6Pressed = false;

        // --- POSICIONAMENTO AUTOMÁTICO BASEADO NO OBJETO PRINCIPAL (cena[0]) ---
        glm::vec3 mainPos = cena[0].posicao;
        float s = cena[0].escala.x; // Escala ajusta a distância proporcionalmente
        
        // Posicionamento técnico clássico de 3 pontos
        glm::vec3 keyLightPos  = mainPos + glm::vec3(-4.0f * s, 3.0f * s, 4.0f * s);  // Frente-Esquerda-Alto
        glm::vec3 fillLightPos = mainPos + glm::vec3(4.0f * s, 1.0f * s, 3.0f * s);   // Frente-Direita-Baixo
        glm::vec3 backLightPos = mainPos + glm::vec3(-1.0f * s, 3.0f * s, -4.0f * s); // Atrás-Alto

        glUseProgram(shaderProgram);

        // Uniforms globais da cena
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Configuração da Luz Principal (Key Light) - Mais intensa
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLights[0].position"), 1, glm::value_ptr(keyLightPos));
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLights[0].color"), 1.0f, 0.95f, 0.9f); 
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[0].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[0].linear"), 0.09f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[0].quadratic"), 0.032f);
        glUniform1i(glGetUniformLocation(shaderProgram, "pointLights[0].enabled"), lightStates[0]);

        // Configuração da Luz de Preenchimento (Fill Light) - Suave para clarear sombras
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLights[1].position"), 1, glm::value_ptr(fillLightPos));
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLights[1].color"), 0.3f, 0.35f, 0.4f); // Tom levemente frio
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[1].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[1].linear"), 0.09f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[1].quadratic"), 0.032f);
        glUniform1i(glGetUniformLocation(shaderProgram, "pointLights[1].enabled"), lightStates[1]);

        // Configuração da Luz de Fundo (Back Light) - Destaca a silhueta do fundo
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLights[2].position"), 1, glm::value_ptr(backLightPos));
        glUniform3f(glGetUniformLocation(shaderProgram, "pointLights[2].color"), 0.6f, 0.6f, 0.6f); 
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[2].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[2].linear"), 0.09f);
        glUniform1f(glGetUniformLocation(shaderProgram, "pointLights[2].quadratic"), 0.032f);
        glUniform1i(glGetUniformLocation(shaderProgram, "pointLights[2].enabled"), lightStates[2]);

        // --- RENDERIZAÇÃO DOS OBJETOS ---
        for (int i = 0; i < cena.size(); i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cena[i].posicao);
            model = glm::rotate(model, cena[i].rotacao.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, cena[i].escala);

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            GLuint locColor = glGetUniformLocation(shaderProgram, "highlightColor");
            if (i == objSelecionado) {
                glUniform3f(locColor, 1.0f, 1.0f, 0.0f); // Destaque amarelo para objeto selecionado
            } else {
                glUniform3f(locColor, 1.0f, 1.0f, 1.0f);
            }

            if (cena[i].textureID != 0) {
                glBindTexture(GL_TEXTURE_2D, cena[i].textureID);
            }

            glBindVertexArray(cena[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, cena[i].nVertices);
        }
        
        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    for (int i = 0; i < cena.size(); i++) {
        glDeleteVertexArrays(1, &cena[i].VAO);
    }
    
    glfwTerminate();
    return 0;
}

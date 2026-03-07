/* Hello Triangle - código adaptado de https://learnopengl.com/#!Getting-started/Hello-Triangle
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Versão inicial: 7/4/2017
 * Última atualização em 07/03/2025
 */

#include <iostream>
#include <string>
#include <assert.h>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Protótipo da função de callback de teclado
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// Protótipos das funções
int setupShader();
int setupGeometry();

// Dimensões da janela
const GLuint WIDTH = 1000, HEIGHT = 1000;

// Variáveis de controle de transformação
float translateX = 0.0f, translateY = 0.0f, translateZ = 0.0f;
float scaleUniform = 1.0f;
bool rotateX = false, rotateY = false, rotateZ = false;

// Código fonte do Vertex Shader com VIEW e PROJECTION adicionados
const GLchar* vertexShaderSource = "#version 450\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"   gl_Position = projection * view * model * vec4(position, 1.0);\n"
"   finalColor = vec4(color, 1.0);\n"
"}\0";

// Código fonte do Fragment Shader
const GLchar* fragmentShaderSource = "#version 450\n"
"in vec4 finalColor;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"   color = finalColor;\n"
"}\n\0";


// Função MAIN
int main()
{
    // Inicialização da GLFW
    glfwInit();

    // Criação da janela GLFW
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Ola 3D -- Enrico!", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // Fazendo o registro da função de callback para a janela GLFW
    glfwSetKeyCallback(window, key_callback);

    // GLAD: carrega todos os ponteiros d funções da OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
    }

    // Obtendo as informações de versão
    const GLubyte* renderer = glGetString(GL_RENDERER); 
    const GLubyte* version = glGetString(GL_VERSION); 
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Compilando e buildando o programa de shader
    GLuint shaderID = setupShader();

    // Gerando o buffer com a geometria do CUBO (36 vértices)
    GLuint VAO = setupGeometry();

    glUseProgram(shaderID);

    // Pegando as localizações das matrizes no shader
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");

    // Configurando a Câmera (View)
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // Configurando a Projeção (Perspectiva 3D)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    // Posições dos múltiplos cubos na cena
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f), // Cubo 0 (Interativo)
        glm::vec3( 2.0f,  1.5f, -2.0f), // Cubo 1 (Cenário)
        glm::vec3(-2.0f, -1.0f, -3.0f)  // Cubo 2 (Cenário)
    };

    // Loop da aplicação - "game loop"
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Limpa o buffer de cor e o buffer de profundidade (DEPTH_BUFFER)
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Processamento contínuo de input para Translação e Escala
        float speed = 0.05f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) translateZ -= speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) translateZ += speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) translateX -= speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) translateX += speed;
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) translateY += speed;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) translateY -= speed;
        
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) scaleUniform -= 0.01f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) scaleUniform += 0.01f;

        float angle = (GLfloat)glfwGetTime();

        glBindVertexArray(VAO);

        // Renderiza múltiplos cubos
        for(unsigned int i = 0; i < 3; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);

            if (i == 0) {
                // Aplica transformações interativas apenas no primeiro cubo
                model = glm::translate(model, cubePositions[i] + glm::vec3(translateX, translateY, translateZ));
                
                if (rotateX) model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
                if (rotateY) model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                if (rotateZ) model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
                
                model = glm::scale(model, glm::vec3(scaleUniform));
            } else {
                // Cubos estáticos apenas são posicionados nos seus devidos lugares
                model = glm::translate(model, cubePositions[i]);
            }

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            
            // Desenha os 36 vértices do cubo (12 triângulos)
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }
    
    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

// Função de callback de teclado para alternar estados com único clique
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        rotateX = !rotateX; // Alterna o estado (liga/desliga)
        rotateY = false;
        rotateZ = false;
    }
    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        rotateX = false;
        rotateY = !rotateY;
        rotateZ = false;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        rotateX = false;
        rotateY = false;
        rotateZ = !rotateZ;
    }
}

int setupShader()
{
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    // Shader Program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

int setupGeometry()
{
    // 36 vértices do CUBO - Cada face com uma cor distinta
    GLfloat vertices[] = {
        // Face Traseira (Vermelho)
        //x     y     z      r    g    b
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,

        // Face Frontal (Verde)
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f,

        // Face Esquerda (Azul)
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,

        // Face Direita (Amarelo)
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,

        // Face Inferior (Ciano)
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,

        // Face Superior (Magenta)
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f
    };

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Atributo posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo cor (r, g, b)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}
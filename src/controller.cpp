// controller.cpp - cliente de controle (roda no seu PC)
// Compilar:  g++ -o controller controller.cpp -std=c++17 -Wall -Wextra
// Executar:  ./controller

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

constexpr const char* SERVER_IP   = "127.0.0.1";  // ← altere para o IP real do servidor
constexpr int        SERVER_PORT  = 44445;
constexpr size_t     BUFFER_SIZE  = 16384;

volatile sig_atomic_t running = 1;

void sigint_handler(int) {
    running = 0;
}

bool send_all(int sock, const std::string& data) {
    size_t total = 0;
    while (total < data.size()) {
        ssize_t sent = send(sock, data.data() + total, data.size() - total, MSG_NOSIGNAL);
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

std::string receive_all(int sock) {
    std::string result;
    char buf[BUFFER_SIZE];
    while (running) {
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        buf[n] = '\0';
        result += buf;
        // Se o servidor enviar saída muito grande, continua lendo até esvaziar
        // (não é perfeito, mas funciona melhor que ler só uma vez)
        if (n < static_cast<ssize_t>(sizeof(buf)-1)) break;
    }
    return result;
}

int main() {
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Erro ao criar socket\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        std::cerr << "Endereço inválido: " << SERVER_IP << "\n";
        close(sock);
        return 1;
    }

    std::cout << "Conectando a " << SERVER_IP << ":" << SERVER_PORT << " ... ";
    std::cout.flush();

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "falhou (" << strerror(errno) << ")\n";
        close(sock);
        return 1;
    }

    std::cout << "conectado!\n\n";

    std::cout << "Comandos disponíveis:\n"
              << "  /list                → listar bots conectados\n"
              << "  /use <parte-do-ip>   → selecionar um bot específico\n"
              << "  /use all             → voltar a enviar para todos\n"
              << "  exit / quit          → sair\n"
              << "  Qualquer outro texto → executado no bot selecionado (ou em todos)\n\n";

    std::string prompt = "> ";

    while (running) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) break;

        if (!running) break;

        // Remove espaços extras no final
        while (!line.empty() && std::isspace(line.back())) line.pop_back();
        if (line.empty()) continue;

        std::string cmd = line;

        if (cmd == "exit" || cmd == "quit") {
            break;
        }

        if (cmd == "/list" || cmd.rfind("/use ", 0) == 0) {
            // Comandos de controle → envia como está
        } else {
            // Comandos normais → envia direto
        }

        if (!send_all(sock, cmd + "\n")) {
            std::cerr << "\nConexão perdida ao enviar.\n";
            break;
        }

        std::string response = receive_all(sock);
        if (response.empty() && !running) break;

        if (!response.empty()) {
            std::cout << response;
            if (response.back() != '\n') std::cout << "\n";
        } else {
            std::cout << "(sem resposta ou conexão fechada)\n";
            break;
        }
    }

    std::cout << "\nDesconectando...\n";
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}
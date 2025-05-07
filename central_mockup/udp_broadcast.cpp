#include <gtkmm.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <json/json.h>

class UDPBroadcastApp : public Gtk::Window {
public:
    UDPBroadcastApp();
    virtual ~UDPBroadcastApp();

protected:
    // Widgets
    Gtk::Box m_main_box;
    Gtk::Frame m_single_frame, m_periodic_frame, m_receive_frame, m_log_frame;
    Gtk::Label m_message_label, m_periodic_label, m_interval_label;
    Gtk::Entry m_message_entry, m_periodic_message_entry, m_interval_entry;
    Gtk::Button m_send_button, m_start_periodic_button, m_stop_periodic_button;
    Gtk::Button m_start_button, m_stop_button, m_clear_button;
    
    // JSON command widgets
    Gtk::Label m_json_label, m_param_label, m_value_label;
    Gtk::Entry m_json_command_entry, m_param_entry, m_value_entry;
    Gtk::Button m_json_button, m_json_control_button;
    
    Gtk::ScrolledWindow m_scrolled_window;
    Gtk::TextView m_log_view;
    Glib::RefPtr<Gtk::TextBuffer> m_log_buffer;
    
    // Variables
    int m_port;
    bool m_running;
    bool m_periodic_sending;
    std::thread m_receiver_thread;
    std::thread m_periodic_thread;
    
    // Thread-safe queue for logging
    std::queue<std::string> m_log_queue;
    std::mutex m_log_mutex;
    std::condition_variable m_log_cond;
    
    // Methods
    void send_udp_message(const std::string& message);
    void log_message(const std::string& message);
    void process_log_queue();
    
    // Signal handlers
    void on_send_button_clicked();
    void on_start_periodic_clicked();
    void on_stop_periodic_clicked();
    void on_start_receiving_clicked();
    void on_stop_receiving_clicked();
    void on_clear_button_clicked();
    void on_json_button_clicked();
    void on_json_control_button_clicked();
    
    // Thread functions
    void receive_messages();
    void periodic_send(const std::string& message, int interval);
};

UDPBroadcastApp::UDPBroadcastApp() 
    : m_main_box(Gtk::ORIENTATION_VERTICAL, 5),
      m_port(12345),
      m_running(false),
      m_periodic_sending(false) {
    
    // Window setup
    set_title("UDP Broadcast Tool");
    set_default_size(600, 600);
    
    // Main container
    add(m_main_box);
    
    // Single send frame
    m_single_frame.set_label("Envio Único");
    m_main_box.pack_start(m_single_frame, Gtk::PACK_SHRINK);
    
    Gtk::Box single_box(Gtk::ORIENTATION_HORIZONTAL, 5);
    m_single_frame.add(single_box);
    
    m_message_label.set_label("Mensagem:");
    single_box.pack_start(m_message_label, Gtk::PACK_SHRINK);
    
    m_message_entry.set_hexpand(true);
    single_box.pack_start(m_message_entry, Gtk::PACK_EXPAND_WIDGET);
    
    m_send_button.set_label("Enviar");
    m_send_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_send_button_clicked));
    single_box.pack_start(m_send_button, Gtk::PACK_SHRINK);
    
    // Periodic send frame
    m_periodic_frame.set_label("Envio Periódico");
    m_main_box.pack_start(m_periodic_frame, Gtk::PACK_SHRINK);
    
    Gtk::Grid periodic_grid;
    m_periodic_frame.add(periodic_grid);
    
    m_periodic_label.set_label("Mensagem:");
    periodic_grid.attach(m_periodic_label, 0, 0, 1, 1);
    
    m_periodic_message_entry.set_text("{\"URI\":\"200/20\"}");
    periodic_grid.attach(m_periodic_message_entry, 1, 0, 2, 1);
    
    m_interval_label.set_label("Intervalo (s):");
    periodic_grid.attach(m_interval_label, 0, 1, 1, 1);
    
    m_interval_entry.set_text("5");
    periodic_grid.attach(m_interval_entry, 1, 1, 1, 1);
    
    m_start_periodic_button.set_label("Iniciar");
    m_start_periodic_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_start_periodic_clicked));
    periodic_grid.attach(m_start_periodic_button, 0, 2, 1, 1);
    
    m_stop_periodic_button.set_label("Parar");
    m_stop_periodic_button.set_sensitive(false);
    m_stop_periodic_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_stop_periodic_clicked));
    periodic_grid.attach(m_stop_periodic_button, 1, 2, 1, 1);
    
    // Receive frame
    m_receive_frame.set_label("Recepção");
    m_main_box.pack_start(m_receive_frame, Gtk::PACK_SHRINK);
    
    Gtk::Box receive_box(Gtk::ORIENTATION_HORIZONTAL, 5);
    m_receive_frame.add(receive_box);
    
    m_start_button.set_label("Iniciar");
    m_start_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_start_receiving_clicked));
    receive_box.pack_start(m_start_button, Gtk::PACK_SHRINK);
    
    m_stop_button.set_label("Parar");
    m_stop_button.set_sensitive(false);
    m_stop_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_stop_receiving_clicked));
    receive_box.pack_start(m_stop_button, Gtk::PACK_SHRINK);
    
    // Log frame
    m_log_frame.set_label("Log");
    m_main_box.pack_start(m_log_frame, Gtk::PACK_EXPAND_WIDGET);
    
    m_scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_log_frame.add(m_scrolled_window);
    
    m_log_buffer = Gtk::TextBuffer::create();
    m_log_view.set_buffer(m_log_buffer);
    m_log_view.set_editable(false);
    m_scrolled_window.add(m_log_view);
    
    m_clear_button.set_label("Limpar");
    m_clear_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_clear_button_clicked));
    m_log_frame.add(m_clear_button);
    
    // JSON command section
    m_json_label.set_label("Comando URI=100/10 (0 para desligar, 1 para ligar):");
    m_main_box.pack_start(m_json_label, Gtk::PACK_SHRINK);
    
    m_json_command_entry.set_text("1");
    m_main_box.pack_start(m_json_command_entry, Gtk::PACK_SHRINK);
    
    m_json_button.set_label("Enviar JSON de comando");
    m_json_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_json_button_clicked));
    m_main_box.pack_start(m_json_button, Gtk::PACK_SHRINK);
    
    // JSON control section
    m_param_label.set_label("Controle URI=100/11 com Parâmetro (CALMA ou WATCHDOG):");
    m_main_box.pack_start(m_param_label, Gtk::PACK_SHRINK);
    
    m_param_entry.set_text("CALMA");
    m_main_box.pack_start(m_param_entry, Gtk::PACK_SHRINK);
    
    m_value_label.set_label("Valor (ex: 1.0):");
    m_main_box.pack_start(m_value_label, Gtk::PACK_SHRINK);
    
    m_value_entry.set_text("1.0");
    m_main_box.pack_start(m_value_entry, Gtk::PACK_SHRINK);
    
    m_json_control_button.set_label("Enviar JSON de Controle");
    m_json_control_button.signal_clicked().connect(sigc::mem_fun(*this, &UDPBroadcastApp::on_json_control_button_clicked));
    m_main_box.pack_start(m_json_control_button, Gtk::PACK_SHRINK);
    
    // Start log processing
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &UDPBroadcastApp::process_log_queue), 100);
    
    show_all_children();
}

UDPBroadcastApp::~UDPBroadcastApp() {
    m_running = false;
    m_periodic_sending = false;
    
    if (m_receiver_thread.joinable()) {
        m_receiver_thread.join();
    }
    
    if (m_periodic_thread.joinable()) {
        m_periodic_thread.join();
    }
}

void UDPBroadcastApp::send_udp_message(const std::string& message) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_message("Erro ao criar socket");
        return;
    }
    
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) {
        log_message("Erro ao configurar broadcast");
        close(sockfd);
        return;
    }
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(m_port);
    servaddr.sin_addr.s_addr = inet_addr("192.168.1.255");
    
    if (sendto(sockfd, message.c_str(), message.length(), 0, 
               (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        log_message("Erro ao enviar mensagem");
    } else {
        log_message("Mensagem enviada:\n" + message);
    }
    
    close(sockfd);
}

void UDPBroadcastApp::log_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_log_mutex);
    m_log_queue.push(message);
    m_log_cond.notify_one();
}

bool UDPBroadcastApp::process_log_queue() {
    std::unique_lock<std::mutex> lock(m_log_mutex);
    
    while (!m_log_queue.empty()) {
        std::string message = m_log_queue.front();
        m_log_queue.pop();
        
        lock.unlock();
        
        auto end_iter = m_log_buffer->end();
        m_log_buffer->insert(end_iter, message + "\n");
        
        // Scroll to end
        auto mark = m_log_buffer->create_mark(end_iter);
        m_log_view.scroll_to(mark);
        m_log_buffer->delete_mark(mark);
        
        lock.lock();
    }
    
    return true; // Continue calling this function
}

void UDPBroadcastApp::on_send_button_clicked() {
    std::string message = m_message_entry.get_text();
    if (message.empty()) {
        log_message("Mensagem vazia. Nada foi enviado.");
        return;
    }
    send_udp_message(message);
}

void UDPBroadcastApp::on_start_periodic_clicked() {
    if (m_periodic_sending) {
        log_message("Envio periódico já está em execução.");
        return;
    }
    
    std::string message = m_periodic_message_entry.get_text();
    if (message.empty()) {
        log_message("Mensagem periódica vazia. Nada será enviado.");
        return;
    }
    
    int interval;
    try {
        interval = std::stoi(m_interval_entry.get_text());
        if (interval <= 0) {
            log_message("Intervalo inválido. Deve ser maior que 0.");
            return;
        }
    } catch (...) {
        log_message("Intervalo inválido. Insira um número.");
        return;
    }
    
    m_periodic_sending = true;
    m_start_periodic_button.set_sensitive(false);
    m_stop_periodic_button.set_sensitive(true);
    
    m_periodic_thread = std::thread(&UDPBroadcastApp::periodic_send, this, message, interval);
    log_message("Envio periódico iniciado.");
}

void UDPBroadcastApp::on_stop_periodic_clicked() {
    m_periodic_sending = false;
    m_start_periodic_button.set_sensitive(true);
    m_stop_periodic_button.set_sensitive(false);
    
    if (m_periodic_thread.joinable()) {
        m_periodic_thread.join();
    }
    
    log_message("Envio periódico parado.");
}

void UDPBroadcastApp::on_start_receiving_clicked() {
    if (m_running) {
        log_message("Recepção já está em execução.");
        return;
    }
    
    m_running = true;
    m_start_button.set_sensitive(false);
    m_stop_button.set_sensitive(true);
    
    m_receiver_thread = std::thread(&UDPBroadcastApp::receive_messages, this);
    log_message("Recepção iniciada.");
}

void UDPBroadcastApp::on_stop_receiving_clicked() {
    m_running = false;
    m_start_button.set_sensitive(true);
    m_stop_button.set_sensitive(false);
    
    if (m_receiver_thread.joinable()) {
        m_receiver_thread.join();
    }
    
    log_message("Recepção parada.");
}

void UDPBroadcastApp::on_clear_button_clicked() {
    m_log_buffer->set_text("");
}

void UDPBroadcastApp::on_json_button_clicked() {
    int comando;
    try {
        comando = std::stoi(m_json_command_entry.get_text());
        if (comando != 0 && comando != 1) {
            log_message("Comando inválido. Use 0 para desligar ou 1 para ligar.");
            return;
        }
    } catch (...) {
        log_message("Comando inválido. Insira um número (0 ou 1).");
        return;
    }
    
    Json::Value json_message;
    json_message["URI"] = "100/10";
    json_message["idAtuador"] = 5;
    json_message["comando"] = comando;
    
    Json::StreamWriterBuilder builder;
    send_udp_message(Json::writeString(builder, json_message));
}

void UDPBroadcastApp::on_json_control_button_clicked() {
    std::string parametro = m_param_entry.get_text();
    if (parametro.empty()) {
        log_message("Parâmetro inválido. Insira um valor válido.");
        return;
    }
    
    double valor;
    try {
        valor = std::stod(m_value_entry.get_text());
    } catch (...) {
        log_message("Valor inválido. Insira um número válido para o campo 'valor'.");
        return;
    }
    
    Json::Value json_message;
    json_message["URI"] = "100/11";
    json_message["idAtuador"] = 5;
    json_message["parametro"] = parametro;
    json_message["valor"] = valor;
    json_message["ACK"] = true;
    
    Json::StreamWriterBuilder builder;
    send_udp_message(Json::writeString(builder, json_message));
}

void UDPBroadcastApp::periodic_send(const std::string& message, int interval) {
    while (m_periodic_sending) {
        send_udp_message(message);
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
}

void UDPBroadcastApp::receive_messages() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_message("Erro ao criar socket para recepção");
        return;
    }
    
    // Allow multiple sockets to use the same PORT number
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) {
        log_message("Erro ao configurar socket para reuso");
        close(sockfd);
        return;
    }
    
    // Set broadcast
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))) {
        log_message("Erro ao configurar broadcast");
        close(sockfd);
        return;
    }
    
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(m_port);
    
    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        log_message("Erro ao vincular socket");
        close(sockfd);
        return;
    }
    
    log_message("Escutando mensagens UDP na porta " + std::to_string(m_port) + "...");
    
    char buffer[1024];
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    
    while (m_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        
        if (retval == -1) {
            log_message("Erro no select");
            break;
        } else if (retval) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                            (struct sockaddr*)&cliaddr, &len);
            if (n > 0) {
                buffer[n] = '\0';
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cliaddr.sin_addr, ipstr, sizeof(ipstr));
                std::string msg = "Mensagem recebida de " + std::string(ipstr) + ":\n" + buffer;
                log_message(msg);
            }
        }
    }
    
    close(sockfd);
}

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.udpbroadcast");
    
    UDPBroadcastApp udp_app;
    return app->run(udp_app);
}
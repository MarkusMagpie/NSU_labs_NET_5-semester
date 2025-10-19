#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <netdb.h> // gethostbyname, 

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100

#define SOCKS_VERSION 5
#define SOCKS_CMD_CONNECT 1
#define SOCKS_ATYP_IPV4 1
#define SOCKS_ATYP_DOMAIN 3

typedef struct {
    int fd; // дескриптор сокета клиента
    int target_fd; // дескриптор сокета целевого сервера
    int state; // состояние клиента: 0 - ожидание "Client greeting"; 1 - ожидание запроса на подключение; 2 - подключен к целевому серверу
    struct sockaddr_in dest_addr;
    char buffer[BUFFER_SIZE];
    size_t buf_len; 
} client_state_t;



void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0); // беру текущие флаги установленные для дескриптора
    fcntl(fd, F_SETFL, flags | O_NONBLOCK); // добавляю к установленным флагам O_NONBLOCK
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("[main] неверное количество аргументов, замена на дефолтные: port: 1080\n");
        argv[1] = "1080";
    }

    int port = atoi(argv[1]);

    // создаем TCP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        domain - AF_INET - протокол IPv4
        type - SOCK_STREAM - TCP
        protocol - 0 - выбирается автоматически
    */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("[main] socket creation failed");
        exit(EXIT_FAILURE);
    }

    // устанавливаю неблокирующий режим серверному сокету
    set_nonblock(server_fd);

    // SO_REUSEADDR позволяет переиспользовать порт сразу после завершения сервера. даже если он в состоянии TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("[main] setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // настройка адреса сервера server_addr
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет (IPv4)
    server_addr.sin_port = htons(port); // номер порта (в сетевом порядке байт) к которому будет привязан сокет

    // привязка сокета к адресу server_addr
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("[main] bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // начало прослушивания (5 потому что в мане прочитал что обычно <= 5)
    if (listen(server_fd, 5) < 0) {
        printf("[main] listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[main] SOCKS5 прокси сервер слушает порт %d...\n", port);

    // массив структур состояний клиентов
    client_state_t clients[MAX_CLIENTS] = {0};
    // максимальный номер дескриптора в множестве. select() ТРЕБУЕТ знать диапазон проверяемых дескрипторов - от 0 до max_fd включительно!!!
    int max_fd = server_fd; 
    // наборы дескрипторы сокетов для чтения и записи
    fd_set read_fds, temp_read, write_fds, temp_write;

    while (1) {
        // очистка дескрипторов и обновление максимального дескриптора перед каждым select()
        FD_ZERO(&temp_read);
        FD_ZERO(&temp_write);
        max_fd = server_fd;
        
        // добавил server_fd в temp_read чтобы следить за тем, когда на server_fd появится новое входящее соединение
        FD_SET(server_fd, &temp_read);
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            // active clients
            if (clients[i].fd > 0) {
                // монитор на чтение данных от клиента 
                FD_SET(clients[i].fd, &temp_read);
                max_fd = (max_fd > clients[i].fd) ? max_fd : clients[i].fd;
                
                // если у клиента есть данные для отправки на целевой сервер, добавляю целевой дескриптор в temp_write для прослушивания на возможность записать в него
                if (clients[i].buf_len > 0 && clients[i].target_fd > 0) {
                    FD_SET(clients[i].target_fd, &temp_write);
                    max_fd = (max_fd > clients[i].target_fd) ? max_fd : clients[i].target_fd;
                }
            }
            
            // active target servers
            if (clients[i].target_fd > 0) {
                // монитор на чтение данные от целевого сервера клиентом
                FD_SET(clients[i].target_fd, &temp_read);
                max_fd = (max_fd > clients[i].target_fd) ? max_fd : clients[i].target_fd;
                
                // если есть данные для отправки клиенту, добавляю дескриптор клиента в temp_write для прослушивания на возможность записать в него
                if (clients[i].buf_len > 0) {
                    FD_SET(clients[i].fd, &temp_write);
                    max_fd = (max_fd > clients[i].fd) ? max_fd : clients[i].fd;
                }
            }
        }

        int activity = select(max_fd + 1, &temp_read, &temp_write, NULL, NULL);
        if (activity < 0) {
            printf("select failed");
            exit(EXIT_FAILURE);
        }
        // temp_read, temp_write модифицированы - выставлены биты только для тех дескрипторов, на которых есть готовность к чтению

        // обработка нового подключения (FD_ISSET - установлен ли бит для именно слушающего сокета server_fd?)
        // да -> в очереди есть входящее подключение (connect() от клиента)
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        if (FD_ISSET(server_fd, &temp_read)) {
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd < 0) {
                printf("accept failed");
                exit(EXIT_FAILURE);
            }
            
            if (client_fd > 0) {
                set_nonblock(client_fd);
                
                // ищу свободный слот для клиента
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        slot = i;
                        break;
                    }
                }

                if (slot != -1) {
                    clients[slot].fd = client_fd;
                    clients[slot].state = 0; // 0 - начальное состояние - 3 way handshake
                    clients[slot].buf_len = 0;
                    clients[slot].target_fd = 0; // пока нет целевого сервера 
                    
                    printf("\n[main] новое подключение от %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                } else {
                    close(client_fd);
                    printf("\n[main] превышено максимальное количество клиентов. Клиент %s:%d отключен\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }
            }
        }

        // обработка активных клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd <= 0) continue;
            
            // FD_ISSET(clients[i].fd, &temp_read) <=> на этом конкретном клиентском сокете есть данные от клиента! (утановлена единичка в temp_read)
            if (FD_ISSET(clients[i].fd, &temp_read)) {
                ssize_t bytes_read = recv(clients[i].fd, clients[i].buffer + clients[i].buf_len, BUFFER_SIZE - clients[i].buf_len, 0);
                
                // ошибка или клиент отключился 
                if (bytes_read <= 0) {
                    getpeername(clients[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                    printf("[main] клиент отключен: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    // закрытие соединения + очистка элемента массива происходит в обоих случаях

                    close(clients[i].fd);
                    close(clients[i].target_fd);

                    clients[i].fd = 0;
                    clients[i].target_fd = 0;

                    FD_CLR(clients[i].fd, &read_fds);
                    FD_CLR(clients[i].fd, &write_fds);
                    FD_CLR(clients[i].target_fd, &read_fds);
                    FD_CLR(clients[i].target_fd, &write_fds);

                    continue;
                }
                
                clients[i].buf_len += bytes_read;
                


                /*обработка SOCKS5 Client greeting: VER, NAUTH, AUTH:
                    VER: SOCKS версия
                    NAUTH: количество методов авторизации
                    AUTH: метод(ы) авторизации
                */
                if (clients[i].state == 0 && clients[i].buf_len >= 3) {
                    printf("[debug] SOCKS5 Client greeting от %s:%d: ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    for (int j = 0; j < clients[i].buf_len && j < 10; j++) {
                        printf("%02X ", (unsigned char)clients[i].buffer[j]);
                    }
                    printf("\n");

                    // проверка версии SOCKS - хачу 5
                    if (clients[i].buffer[0] != SOCKS_VERSION) {
                        printf("[debug] ошибка версии SOCKS. Ожидается %d, получено %d\n", SOCKS_VERSION, clients[i].buffer[0]);
                        close(clients[i].fd);
                        clients[i].fd = 0;
                        continue;
                    }
                    
                    /*отрпавка клиенту Server choice: VER, CAUTH:
                        VER: SOCKS версия
                        CAUTH: выбранный метод авторизации
                    */
                    char response[] = {SOCKS_VERSION, 0x00};
                    send(clients[i].fd, response, sizeof(response), 0);

                    printf("[debug] SOCKS5 Server choice для %s:%d: ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    for (int j = 0; j < sizeof(response); j++) {
                        printf("%02X ", (unsigned char)response[j]);
                    }
                    printf("\n");
                    
                    clients[i].state = 1; // новое состояние клиента - состояние запроса
                    clients[i].buf_len = 0;
                }
                
                /*обработка SOCKS5 "Client connection request": VER, CMD, RSV, ATYP, DST.ADDR, DST.PORT
                    VER: SOCKS версия
                    CMD: команда (0x01: establish a TCP/IP stream connection)
                    RSV: зарезервировано (0x00) 
                    ATYP: тип адреса (0x01: IPv4)
                    DST.ADDR: целевой IP-адрес 
                    DST.PORT: целевой порт 
                */ 
                if (clients[i].state == 1 && clients[i].buf_len >= 10) {
                    printf("[debug] SOCKS5 Client connection request от %s:%d: ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    for (int j = 0; j < 10; j++) {
                        printf("%02X ", (unsigned char)clients[i].buffer[j]);
                    }
                    printf("\n");

                    // проверка CMD 
                    if (clients[i].buffer[1] != SOCKS_CMD_CONNECT) {
                        printf("[debug] ошибка в CMD SOCKS Client connection request. Ожидается %d, получено %d\n", SOCKS_CMD_CONNECT, clients[i].buffer[1]);
                        char response[] = {SOCKS_VERSION, 0x07};
                        
                        send(clients[i].fd, response, sizeof(response), 0);
                        
                        close(clients[i].fd);
                        clients[i].fd = 0;
                        continue;
                    }
                    
                    // обработка типа адреса - либо IPv4 либо надо резолвить доменное имя
                    uint8_t atyp = clients[i].buffer[3];
                    if (atyp == SOCKS_ATYP_IPV4) {
                        // извлечение IPv4 адреса
                        memcpy(&clients[i].dest_addr.sin_addr, &clients[i].buffer[4], 4);
                        clients[i].dest_addr.sin_port = *(uint16_t*)&clients[i].buffer[8];
                        clients[i].dest_addr.sin_family = AF_INET;
                        
                        int target_fd = socket(AF_INET, SOCK_STREAM, 0);
                        if (target_fd < 0) {
                            printf("[debug] socket creation failed");
                            close(clients[i].fd);
                            clients[i].fd = 0;
                            continue;
                        }
                        
                        set_nonblock(target_fd);
                        
                        // в неблокирующем режиме connect возвращает EINPROGRESS - соединение начато, но не завершено (в процессе) -> не считаю ошибкой
                        if ((connect(target_fd, (struct sockaddr*)&clients[i].dest_addr, sizeof(clients[i].dest_addr)) < 0) && (errno != EINPROGRESS)) {
                            printf("[debug] connect failed");
                            close(target_fd);
                            close(clients[i].fd);
                            clients[i].fd = 0;
                            continue;
                        }
                        
                        clients[i].target_fd = target_fd;
                        clients[i].state = 2; // подключен к целевому серверу
                        clients[i].buf_len = 0; // очистка
                        
                        /* отправка клиенту SOCKS5 "Response packet from server": VER, STATUS, RSV, BNDADDR, BNDPORT:
                            VER: SOCKS версия
                            STATUS: статус (0x00: request granted)
                            RSV: зарезервировано (0x00)
                            BNDADDR: IPv4 адрес, на который соединение установлено
                            BNDPORT: порт, на который соединение установлено
                        */

                        // "успешный" ответ на запрос клиента
                        char response[10] = {
                            SOCKS_VERSION, 
                            0x00, // status: request granted 
                            0x00, // reserved, must be 0x00
                            SOCKS_ATYP_IPV4
                        };
                        memcpy(response + 4, &clients[i].dest_addr.sin_addr, 4); // копирую в ответ клиенту IPv4 адрес
                        memcpy(response + 8, &clients[i].dest_addr.sin_port, 2); // и порт
                        
                        send(clients[i].fd, response, 10, 0);

                        printf("[debug] SOCKS5 Response packet from server для %s:%d: ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        for (int j = 0; j < sizeof(response); j++) {
                            printf("%02X ", (unsigned char)response[j]);
                        }
                        printf("\n");
                        
                        printf("клиент %s:%d успешно подключился к целевому серверу %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), 
                            inet_ntoa(clients[i].dest_addr.sin_addr), ntohs(clients[i].dest_addr.sin_port));
                    } else if (atyp == SOCKS_ATYP_DOMAIN) {
                        // длина домена
                        uint8_t domain_len = clients[i].buffer[4];
                        
                        // копи домена во временный буфер
                        char domain[256];
                        if (domain_len > sizeof(domain) - 1) {
                            domain_len = sizeof(domain) - 1;
                        }
                        memcpy(domain, &clients[i].buffer[5], domain_len);
                        domain[domain_len] = '\0';
                        
                        // порт (последние 2 байта)
                        uint16_t port = ntohs(*(uint16_t*)&clients[i].buffer[5 + domain_len]);
                        
                        printf("[debug] запрос на подключение к доменному имени: %s:%d\n", domain, port);

                        // теперь непосредственно разрешение доменного имени в IPv4 адрес
                        struct hostent *host = gethostbyname(domain); // возвращает структуру hostent с информацией о domain
                        if (!host || !host->h_addr_list[0]) {
                            printf("[debug] DNS resolution failed");
                            char response[] = {SOCKS_VERSION, 0x04}; // domain name not found

                            send(clients[i].fd, response, sizeof(response), 0);
                            close(clients[i].fd);
                            clients[i].fd = 0;
                            continue;
                        }

                        // копирование первого IPv4 адреса из host->h_addr_list в clients[i].dest_addr чтобы подключиться к этому IP-адресу через connect()
                        memcpy(&clients[i].dest_addr.sin_addr, host->h_addr_list[0], host->h_length);
                        clients[i].dest_addr.sin_port = htons(port);
                        clients[i].dest_addr.sin_family = AF_INET;

                        // сокет для целевого сервера
                        int target_fd = socket(AF_INET, SOCK_STREAM, 0);
                        if (target_fd < 0) {
                            printf("[debug] socket creation failed");
                            close(clients[i].fd);
                            clients[i].fd = 0;
                            continue;
                        }
                        
                        set_nonblock(target_fd);

                        // в неблокирующем режиме connect возвращает EINPROGRESS - соединение начато, но не завершено (в процессе) -> не считаю ошибкой
                        if ((connect(target_fd, (struct sockaddr*)&clients[i].dest_addr, sizeof(clients[i].dest_addr)) < 0) && (errno != EINPROGRESS)) {
                            printf("[debug] connect failed");
                            close(target_fd);
                            close(clients[i].fd);
                            clients[i].fd = 0;
                            continue;
                        }
                        
                        clients[i].target_fd = target_fd;
                        clients[i].state = 2;
                        clients[i].buf_len = 0;
                        
                        // "успешный" ответ на запрос клиента
                        char response[10] = {
                            SOCKS_VERSION, 
                            0x00, // status: request granted 
                            0x00, // reserved, must be 0x00
                            SOCKS_ATYP_IPV4
                        };
                        memcpy(response + 4, &clients[i].dest_addr.sin_addr, 4); // копирую в ответ клиенту резолвенутый IPv4 адрес
                        memcpy(response + 8, &clients[i].dest_addr.sin_port, 2); // и порт
                        
                        send(clients[i].fd, response, 10, 0);

                        printf("[debug] SOCKS5 Response packet from server для %s:%d: ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        for (int j = 0; j < sizeof(response); j++) {
                            printf("%02X ", (unsigned char)response[j]);
                        }
                        printf("\n");

                        printf("клиент %s:%d успешно подключился к целевому серверу %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), 
                            inet_ntoa(clients[i].dest_addr.sin_addr), ntohs(clients[i].dest_addr.sin_port));
                    } else {
                        // не реализована обработОЧКА других типов
                        printf("[debug] ошибка типа адреса SOCKS. Ожидается %d, получено %d\n", SOCKS_ATYP_IPV4, atyp);

                        char response[] = {SOCKS_VERSION, 0x08}; // address type not supported
                        send(clients[i].fd, response, sizeof(response), 0);
                        close(clients[i].fd);
                        clients[i].fd = 0;
                    }
                }
            }
            
            // отправка данных клиенту от целевого сервера
            // FD_ISSET(clients[i].target_fd, &temp_read) <=> на этом конкретном клиентском сокете есть возможность принять данные от целевого сервера
            if (FD_ISSET(clients[i].target_fd, &temp_read) && clients[i].target_fd > 0) {
                // получаю данные от целевого сервера в временный буфер, проверяю что что-то есть и пересылаю клиенту
                char temp_buf[BUFFER_SIZE];
                ssize_t bytes_read = recv(clients[i].target_fd, temp_buf, BUFFER_SIZE, 0);

                if (bytes_read <= 0) {
                    getpeername(clients[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                    printf("клиент %s:%d отключен\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    // закрытие соединения + очистка элемента массива происходит в обоих случаях

                    close(clients[i].fd);
                    close(clients[i].target_fd);
                    
                    clients[i].fd = 0;
                    clients[i].target_fd = 0;

                    FD_CLR(clients[i].fd, &read_fds);
                    FD_CLR(clients[i].fd, &write_fds);
                    FD_CLR(clients[i].target_fd, &read_fds);
                    FD_CLR(clients[i].target_fd, &write_fds);

                    continue;
                }
                
                // отправка данных клиенту 
                ssize_t bytes_sent = send(clients[i].fd, temp_buf, bytes_read, 0);
                if (bytes_sent < 0) {
                    getpeername(clients[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                    printf("send failed, клиент %s:%d отключен\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    // закрытие соединения + очистка элемента массива происходит в обоих случаях
                    
                    close(clients[i].fd);
                    close(clients[i].target_fd);
                    
                    clients[i].fd = 0;
                    clients[i].target_fd = 0;

                    FD_CLR(clients[i].fd, &read_fds);
                    FD_CLR(clients[i].fd, &write_fds);
                    FD_CLR(clients[i].target_fd, &read_fds);
                    FD_CLR(clients[i].target_fd, &write_fds);

                    continue;
                }
            }
            
            // отправка данных целевому серверу от клиента
            // FD_ISSET(clients[i].target_fd, &temp_write) <=> на этом конкретном клиентском сокете есть возможность отправить данные целевому серверу
            if (FD_ISSET(clients[i].target_fd, &temp_write)) {
                if (clients[i].target_fd > 0 && clients[i].buf_len > 0) {
                    // клиент отправляет данные целевому серверу
                    ssize_t bytes_sent = send(clients[i].target_fd, clients[i].buffer, clients[i].buf_len, 0);

                    // в client_addr хранится имя машины, подключившейся к сокету clients[i].fd
                    getpeername(clients[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                    printf("клиент %s:%d отправил целевому серверу %ld байт\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), bytes_sent);

                    // ошибка ИЛИ сервер отключился 
                    if (bytes_sent <= 0) {
                        getpeername(clients[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                        printf("клиент %s:%d отключен\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        // закрытие соединения + очистка элемента массива происходит в обоих случаях

                        close(clients[i].fd);
                        close(clients[i].target_fd);
                        
                        clients[i].fd = 0;
                        clients[i].target_fd = 0;

                        FD_CLR(clients[i].fd, &read_fds);
                        FD_CLR(clients[i].fd, &write_fds);
                        FD_CLR(clients[i].target_fd, &read_fds);
                        FD_CLR(clients[i].target_fd, &write_fds);

                        continue;
                    }
                    
                    // сдвиг оставшихся данных в буфере на butes_sent байт
                    memmove(clients[i].buffer, clients[i].buffer + bytes_sent, clients[i].buf_len - bytes_sent);
                    clients[i].buf_len -= bytes_sent;
                }
            }
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
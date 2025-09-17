#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <sys/wait.h>
#include <signal.h>

#define MESSAGE "alive"
#define BUFFER_SIZE 1024

void send_multicast_message(char *host, int port, int family) {
    // создаю UDP сокет в зависимости от family
    int sock_fd = socket(family, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        printf("socket creation failed");
        return;
    }

    // сколько маршрутизаторов пройдет пакет - TTL
    if (family == AF_INET) { // IPv4
        int ttl = 2;
        // IP_MULTICAST_TTL - socket option used in network programming to set the Time To Live (TTL) 
        // for outgoing multicast packets, controlling how far they can travel across a network
        if (setsockopt(sock_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
            printf("IPv4: setsockopt failed");
            close(sock_fd);
            return;
        }
    } else { // IPv6
        int hops = 2;
        if (setsockopt(sock_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops)) < 0) {
            printf("IPv6: setsockopt failed");
            close(sock_fd);
            return;
        }
    }

    // отправка UDP пакета в группу мультикаста
    if (family == AF_INET) { // IPv4
        struct sockaddr_in dest4 = {0};
        dest4.sin_family = AF_INET;
        dest4.sin_port = htons(port);
        // pton <-> Presentation to Network - преобразовать IP host-а в сетевой бинарный формат
        if (inet_pton(AF_INET, host, &dest4.sin_addr) != 1) {
            printf("IPv4: не удалось преобразовать IP: %s\n", host);
            close(sock_fd);
            return;
        }

        int bytes_sent = sendto(sock_fd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr*)&dest4, sizeof(dest4));
        if (bytes_sent < 0) {
            printf("IPv4: sendto failed");
            close(sock_fd);
            return;
        }
    } else { // IPv6
        struct sockaddr_in6 dest6 = {0};
        dest6.sin6_family = AF_INET6;
        dest6.sin6_port = htons(port);
        if (inet_pton(AF_INET6, host, &dest6.sin6_addr) != 1) {
            printf("IPv6: не удалось преобразовать IP: %s\n", host);
            close(sock_fd);
            return;
        }

        int bytes_sent = sendto(sock_fd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr*)&dest6, sizeof(dest6));
        if (bytes_sent < 0) {
            printf("IPv6: sendto failed");
            close(sock_fd);
            return;
        }
    }

    close(sock_fd);
}

char **receive_multicast_messages(int sock, int *count) {
    *count = 0; // счетчик найденных IP адресов
    char **list = NULL; // массив IP адресов
    char buf[BUFFER_SIZE]; // буфер для чтения UDP-пакетов по сокету
    struct sockaddr_storage src; // указатель на буфер в структуре sockaddr, который будет содержать исходный адрес при возврате.
    socklen_t srclen = sizeof(src);

    while (1) {
        int bytes_received = recvfrom(sock, buf, BUFFER_SIZE, 0, (struct sockaddr*)&src, &srclen);
        if (bytes_received <= 0) {
            // таймаут, выхожу с накопленным list-ом
            break;
        }

        // src - отправитель UDP пакета, преобразую src->sin_addr в строку и записываю в ipstr
        char ipstr[INET6_ADDRSTRLEN];
        if (src.ss_family == AF_INET) { // IPv4
            inet_ntop(AF_INET, &((struct sockaddr_in*)&src)->sin_addr, ipstr, sizeof(ipstr));
        } else { // IPv6
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)&src)->sin6_addr, ipstr, sizeof(ipstr));
        }

        // расширяю массив list на +1 элемент 
        char **tmp = realloc(list, (*count + 1) * sizeof(char*));
        if (!tmp) {
            printf("realloc failed\n");
            break;
        }
        list = tmp;

        // выделяю память и копирую туда ipstr - IP адрес отправителя
        list[*count] = strdup(ipstr);
        if (!list[*count]) {
            printf("strdup failed\n");
            break;
        }
        (*count)++;
    }

    return list;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("неверное количество аргументов, замена на дефолтные: IPv4-адрес: 239.255.255.254, port: 5000\n");
        argv[1] = "239.255.255.254";
        argv[2] = "5000";
    }

    char *host = argv[1];
    int port = atoi(argv[2]);
    int family = (':' == host[0]) ? AF_INET6 : AF_INET;

    if (family == AF_INET6) {
        printf("слушаем на %s/%d (версия IPv6)\n", host, port);
    } else {
        printf("слушаем на %s/%d (версия IPv4)\n", host, port);
    }

    // создаю UDP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        domain: AF_INET - IPv4; AF_INET6 - IPv6
        type: SOCK_DGRAM - UDP
        protocol: 0 - выбирается автоматически
    */
    int sock_fd = socket(family, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        printf("socket failed");
        exit(EXIT_FAILURE);
    }

    // устанавливаю таймаут на прием сообщений
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("setsockopt failed\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR позволяет переиспользовать порт сразу после завершения сервера. даже если он в состоянии TIME_WAIT
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("setsockopt failed\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
 
    if (family == AF_INET) { // IPv4
        // привязка сокета к bind_addr
        struct sockaddr_in bind_addr;
        bind_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет
        bind_addr.sin_addr.s_addr = INADDR_ANY; // the socket will be bound to all local interfaces 
        bind_addr.sin_port = htons(port); // номер порта (в сетевом порядке байт) будет привязан к сокету
        if (bind(sock_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            printf("IPv4: bind failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        // присоединение к группе мультикаста чтобы получать её UDP пакеты
        /*
        struct ip_mreq {
            imr_multiaddr - адрес multicast группы
            imr_interface - интерфейс для присоединения (INADDR_ANY - любой интерфейс)
        }
        inet_pton преобразует IP-адрес host в бинарный сетевой формат и запишет в m4.imr_multiaddr
        setsockopt() - "хочу получать UDP пакеты, адресованные на этот мультикаст адрес"
        */
        struct ip_mreq m4;
        inet_pton(AF_INET, host, &m4.imr_multiaddr);
        m4.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m4, sizeof(m4)) < 0) {
            printf("IPv4: setsockopt failed\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
    } else { // IPv6
        struct sockaddr_in6 bind_addr;
        bind_addr.sin6_family = AF_INET6;
        bind_addr.sin6_addr = in6addr_any;
        bind_addr.sin6_port = htons(port);
        if (bind(sock_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            printf("IPv6: bind failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        struct ipv6_mreq m6;
        inet_pton(AF_INET6, host, &m6.ipv6mr_multiaddr);
        m6.ipv6mr_interface = 0;
        if (setsockopt(sock_fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &m6, sizeof(m6)) < 0) {
            printf("IPv6: setsockopt failed\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
    }

    while(1) {
        // 1
        send_multicast_message(host, port, family);
        
        // 2
        int count = 0;
        char **res = receive_multicast_messages(sock_fd, &count);

        printf("найдено копий: %d:\n", count);
        for (int i = 0; i < count; i++) {
            printf("\t%s\n", res[i]);
            free(res[i]);
        }
        free(res);

        sleep(1);
    }

    close(sock_fd);
    return 0;
}
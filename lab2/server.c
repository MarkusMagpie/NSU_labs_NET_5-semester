#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <endian.h> // для htobe64
#include <sys/time.h> // struct timeval

#define FILENAME_LENGTH 4096
#define BUFFER_SIZE 1048576
#define UPLOAD_DIR "uploads"
#define REPORT_INTERVAL 3 // секунд

typedef struct {
    int client_fd;
    struct sockaddr_in addr;
} thread_data_t;

ssize_t send_all(int sock_fd, const void *buf, size_t len) {
    ssize_t bytes_sent_total = 0;
    while (bytes_sent_total < len) {
        ssize_t bytes_sent_now = send(sock_fd, buf + bytes_sent_total, len - bytes_sent_total, 0);
        if (bytes_sent_now < 0) { // если при отправке произошла ошибка (оборвался сокет или что-то еще
            return -1;
        }
        bytes_sent_total += bytes_sent_now;
    }
    return bytes_sent_total;
}

ssize_t recv_all(int fd, void *buf, size_t len) {
    size_t bytes_received_total = 0;
    while (bytes_received_total < len) {
        ssize_t bytes_received_now = recv(fd, (char*)buf + bytes_received_total, len - bytes_received_total, 0);
        if (bytes_received_now <= 0) {
            return -1;
        }
        bytes_received_total += bytes_received_now;
    }
    return bytes_received_total;
}

void *handle_client(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    int client_fd = data->client_fd;

    // из структуры thread_data_t извлекаю клиентский IP-адрес и порт
    char client_ip[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN - максимальная длина строки IP-адреса
    inet_ntop(AF_INET, &data->addr.sin_addr, client_ip, sizeof(client_ip)); // inet_ntop - преобразует IP адрес клиента в строковый формат и записывает его в строку client_ip
    int client_port = ntohs(data->addr.sin_port);

    free(data);

    // 1 считать длину имени клиентского файла (4 байта)
    uint32_t net_filename_len;
    if (recv_all(client_fd, &net_filename_len, sizeof(net_filename_len)) < 0) {
        printf("recv failed\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    uint32_t filename_len = ntohl(net_filename_len); // преобразование сетевого порядка в ???
    if (filename_len == 0 || filename_len > FILENAME_LENGTH) {
        printf("filename length error\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 2 считать название клиентского файла
    char *filename = malloc(filename_len + 1);
    if (recv_all(client_fd, filename, filename_len) < 0) {
        printf("recv failed\n");
        free(filename);
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    filename[filename_len] = '\0';

    // отбрезаем путь оставляя только basename
    char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename; // если base == NULL, то base = filename

    // 3 считать длину клиентского файла (8 байт)
    uint64_t net_file_size;
    if (recv_all(client_fd, &net_file_size, 8) < 0) {
        printf("recv failed\n");
        free(filename);
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    uint64_t file_size = be64toh(net_file_size);

    // 4 открыть файл для записи в uploads
    
    // создаём папку uploads если её нет
    if (access(UPLOAD_DIR, F_OK) != 0) {
        if (mkdir(UPLOAD_DIR, 0777) < 0) {
            printf("mkdir failed\n");
            free(filename);
            close(client_fd);
            exit(EXIT_FAILURE);
        }
    }

    char path[FILENAME_LENGTH];
    snprintf(path, sizeof(path), UPLOAD_DIR "/%s", base); // конкатенируем путь к папке и название клиентского файла: uploads/base <-> uploads/test.txt
    printf("сервер копирует клиентский файл в: %s\n", path);
    FILE *out = fopen(path, "wb"); // открываем клиентский файл в режиме записи в бинарном режиме
    if (!out) { 
        printf("fopen failed\n");
        free(filename);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 5 считать содержимое клиентского файла в uploads + раз в 3 секунды выводить в консоль мгновенную скорость приёма и среднюю скорость за сеанс 
    printf("начинаю копирование клиентского файла от клиента [%s:%d]:\n", client_ip, client_port);
    uint64_t now_received = 0, last_received = 0;
    // time_t start_time = time(NULL), last_time = start_time;
    struct timeval start_tv, last_tv, now_tv;
    gettimeofday(&start_tv, NULL);
    last_tv = start_tv;

    char buf[BUFFER_SIZE];
    while (now_received < file_size) {
        ssize_t bytes_received = recv(client_fd, buf, file_size - now_received, 0);
        if (bytes_received <= 0) break;
        fwrite(buf, 1, bytes_received, out);
        printf("   принял %ld байт\n", bytes_received);
        now_received += bytes_received;
        
        // time_t current_time = time(NULL);
        gettimeofday(&now_tv, NULL);
        double interval = (now_tv.tv_sec  - last_tv.tv_sec) + (now_tv.tv_usec - last_tv.tv_usec) / 1e6;
        
        if (interval >= REPORT_INTERVAL) {
            double total = (now_tv.tv_sec  - start_tv.tv_sec) + (now_tv.tv_usec - start_tv.tv_usec) / 1e6;
            double instant = (now_received - last_received) / interval;

            // средняя скорость = всего принятых байт / всего секунд
            double average_speed = now_received / total; 
            // мгновенная скорость = байт за последний интервал / длительность интервала
            double instant_speed = (now_received - last_received) / interval;  
            
            printf("[%s:%d] мгновенная скорость: %.1f байт/сек, средняя скорость: %.1f байт/сек\n", client_ip, client_port, instant_speed, average_speed);
            
            // обновляю последнее время и последнее количество принятых байт
            last_tv = now_tv;
            last_received = now_received;
        }
    }

    fclose(out);

    // time_t now = time(NULL);
    gettimeofday(&now_tv, NULL);
    double interval = (now_tv.tv_sec  - last_tv.tv_sec) + (now_tv.tv_usec - last_tv.tv_usec) / 1e6;
    if (interval < REPORT_INTERVAL) {
        double total = (now_tv.tv_sec  - start_tv.tv_sec) + (now_tv.tv_usec - start_tv.tv_usec) / 1e6;
        double instant = (now_received - last_received) / interval;
        double average_speed  = now_received / total;
        double instant_speed = (now_received - last_received) / interval;
        printf("[%s:%d] мгновенная скорость: %.1f байт/сек, средняя скорость: %.1f байт/сек\n", client_ip, client_port, instant_speed, average_speed);
    }

    // 6 сверить размер файла
    uint8_t ok = (now_received == file_size) ? 1 : 0;

    // 7 отправить клиенту OK или НЕ ОК 
    send_all(client_fd, &ok, 1); // отправляем 1 байт
    printf("[%s:%d] %s: считал %ld байт\n", client_ip, client_port, ok ? "УСПЕХ" : "ОШИБКА", now_received);

    close(client_fd);
    free(filename);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("неверное количество аргументов, замена на дефолтные: port: 5000\n");
        argv[1] = "5000";
    }

    int port = atoi(argv[1]);

    // создаем TCP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        __domain - AF_INET - протокол IPv4
        __type - SOCK_STREAM - TCP
        __protocol - 0 - выбирается автоматически
    */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR позволяет переиспользовать порт сразу после завершения сервера. даже если он в состоянии TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("setsockopt failed\n");
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
        printf("bind failed\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // начало прослушивания (5 потому что в мане прочитал что обычно <= 5)
    if (listen(server_fd, 5) < 0) {
        printf("listen failed\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP сервер запущен на порту %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // принятие нового соединения
        int client_sock_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock_fd < 0) {
            printf("accept failed\n");
            continue;
        }

        printf("новое подключение от %s:%d\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);

        // НОВОЕ: создаю поток (раньше был fork()) для обработки нового соединения
        pthread_t tid;
        thread_data_t* data = malloc(sizeof(thread_data_t));
        if (data == NULL) {
            printf("malloc failed\n");
            close(client_sock_fd);
            continue;
        }

        data->client_fd = client_sock_fd;
        data->addr = client_addr;

        if (pthread_create(&tid, NULL, handle_client, data) < 0) {
            printf("pthread_create failed\n");
            close(client_sock_fd);
            free(data);
        } else {
            pthread_detach(tid);
        }
    }

    close(server_fd);
    return 0;
}
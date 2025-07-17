#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <endian.h> // для htobe64

#define BUFFER_SIZE 4096

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

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("неверное количество аргументов, замена на дефолтные: filename: test.txt, IP: 127.0.0.1, port: 5000\n");
        argv[1] = "test.txt";
        argv[2] = "127.0.0.1";
        argv[3] = "5000";
    }

    char *filename = argv[1];
    char *server_ip = argv[2];
    int port = atoi(argv[3]);
    
    // открываем файл
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("open failed\n");
        exit(EXIT_FAILURE);
    }

    // создаем TCP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        __domain - AF_INET - протокол IPv4
        __type - SOCK_STREAM - TCP
        __protocol - 0 - выбирается автоматически
    */
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        printf("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // настройка адреса сервера server_addr
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет (IPv4)
    server_addr.sin_port = htons(port); // номер порта (в сетевом порядке байт) к которому будет привязан сокет
    
    // конвертируем IP 127.0.0.1 в сетевой порядок байт
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        printf("address conversion failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // подключение к серверу
    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("connect failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    printf("подключился к серверу %s:%d\n", server_ip, port);

    // 1 отправить длину filename
    size_t filename_len = strlen(filename);
    uint32_t net_filename_len = htonl(filename_len); // конвертировал в сетевый порядок байт
    if (send_all(client_fd, &net_filename_len, sizeof(net_filename_len)) < 0) {
        printf("ошибка отправки длины filename\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 2 отправить filename
    if (send_all(client_fd, filename, filename_len) < 0) {
        printf("ошибка отправки filename\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 3 отправить длину файла
    
    // для этого считываю размер файла
    // off_t file_size = lseek(fd, 0, SEEK_END);
    // lseek(fd, 0, SEEK_SET);
    // int net_file_size = htonl(file_size); // конвертировал в сетевый порядок байт

    struct stat st;
    if (fstat(fd, &st) != 0) {
        printf("ошибка получения размера файла");
        close(fd);
        exit(EXIT_FAILURE);
    }
    uint64_t file_size = st.st_size;
    printf("размер файла '%s': %ld байт\n", filename, file_size);

    uint64_t net_file_size = htobe64(file_size); // конвертировал в сетевый порядок байт

    if (send_all(client_fd, &net_file_size, 8) < 0) { // отправил 8 байт
        printf("ошибка отправки длины файла\n");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // 4 отправить содержимое файла filename

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (send_all(client_fd, buffer, bytes_read) < 0) {
            printf("ошибка отправки содержимого файла %s\n", filename);
        }
    }

    printf("отправил серверу файл %s\n", filename);

    // 5 получить результат
    uint8_t result;
    if (recv_all(client_fd, &result, 1) == 1) {
        if (result == 1) 
            printf("УСПЕХ: файл %s отправлен\n", filename);
        else 
            printf("ОШИБКА: файл %s не отправлен\n", filename);
    } else {
        printf("ошибка получения результата\n");
    }

    close(client_fd);
    return 0;
}
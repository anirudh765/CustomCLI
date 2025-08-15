#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include "config.h"
#include "email_utils.h"

// Function to send a command to SMTP server and receive response
void send_command(SSL *ssl, const char *cmd, char *response, size_t response_size)
{
    printf("Sending: %s", cmd);
    SSL_write(ssl, cmd, strlen(cmd));
    int bytes = SSL_read(ssl, response, response_size - 1);
    if (bytes > 0)
    {
        response[bytes] = '\0';
        printf("Received: %s", response);
    }
    else
    {
        printf("No response or error\n");
    }
}

// Base64 encoding for the attachment
char *base64_encode(const unsigned char *buffer, size_t length)
{
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;
    bio = BIO_new(BIO_s_mem());
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    char *base64_data = (char *)malloc(buffer_ptr->length + 1);
    memcpy(base64_data, buffer_ptr->data, buffer_ptr->length);
    base64_data[buffer_ptr->length] = '\0';
    BIO_free_all(bio);
    return base64_data;
}

// Create and send the email with file attachment
void send_email(const char *filename, const char *email, const char *subject)
{

    int file = open(filename, O_RDONLY);
    if (file < 0)
    {
        perror("Failed to open file");
        return;
    }

    struct stat st;
    if (fstat(file, &st) < 0)
    {
        perror("Failed to get file size");
        close(file);
        return;
    }

    long file_size = st.st_size;
    unsigned char *file_content = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (file_content == MAP_FAILED)
    {
        perror("Failed to allocate memory");
        close(file);
        return;
    }

    if (read(file, file_content, file_size) != file_size)
    {
        perror("Failed to read file");
        munmap(file_content, file_size);
        close(file);
        return;
    }

    close(file);

    char *encoded_file = base64_encode(file_content, file_size);
    free(file_content);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to create socket");
        free(encoded_file);
        return;
    }

    struct sockaddr_in smtp_server;
    smtp_server.sin_family = AF_INET;
    struct hostent *server = gethostbyname("smtp.gmail.com");
    if (!server)
    {
        perror("Failed to resolve SMTP server hostname");
        close(sockfd);
        free(encoded_file);
        return;
    }

    memcpy(&smtp_server.sin_addr, server->h_addr, server->h_length);
    smtp_server.sin_port = htons(SMTP_PORT);

    printf("Connecting to SMTP server...\n");
    if (connect(sockfd, (struct sockaddr *)&smtp_server, sizeof(smtp_server)) < 0)
    {
        perror("Failed to connect to SMTP server");
        close(sockfd);
        free(encoded_file);
        return;
    }

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx)
    {
        perror("Failed to create SSL context");
        close(sockfd);
        free(encoded_file);
        return;
    }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL *ssl = SSL_new(ctx);
    if (!ssl)
    {
        perror("Failed to create SSL structure");
        SSL_CTX_free(ctx);
        close(sockfd);
        free(encoded_file);
        return;
    }

    SSL_set_fd(ssl, sockfd);
    printf("Establishing SSL connection...\n");
    if (SSL_connect(ssl) != 1)
    {
        fprintf(stderr, "Failed to establish SSL connection:\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        free(encoded_file);
        return;
    }

    printf("SSL connection established!\n");

    char buffer[1024] = {0};

    SSL_read(ssl, buffer, sizeof(buffer) - 1);
    printf("Server greeting: %s", buffer);

    send_command(ssl, "EHLO client\r\n", buffer, sizeof(buffer));

    send_command(ssl, "AUTH LOGIN\r\n", buffer, sizeof(buffer));

    char *encoded_user = base64_encode((unsigned char *)SMTP_USER, strlen(SMTP_USER));
    char auth_cmd[1024];
    snprintf(auth_cmd, sizeof(auth_cmd), "%s\r\n", encoded_user);
    send_command(ssl, auth_cmd, buffer, sizeof(buffer));
    free(encoded_user);

    char *encoded_password = base64_encode((unsigned char *)SMTP_PASS, strlen(SMTP_PASS));
    snprintf(auth_cmd, sizeof(auth_cmd), "%s\r\n", encoded_password);
    send_command(ssl, auth_cmd, buffer, sizeof(buffer));
    free(encoded_password);

    send_command(ssl, "MAIL FROM:<crazycoder0407@gmail.com>\r\n", buffer, sizeof(buffer));

    char rcpt_cmd[512];
    snprintf(rcpt_cmd, sizeof(rcpt_cmd), "RCPT TO:<%s>\r\n", email);
    send_command(ssl, rcpt_cmd, buffer, sizeof(buffer));

    send_command(ssl, "DATA\r\n", buffer, sizeof(buffer));

    char email_body[8192];
    snprintf(email_body, sizeof(email_body),
             "From: <crazycoder0407@gmail.com>\r\n"
             "To: <%s>\r\n"
             "Subject: %s\r\n"
             "MIME-Version: 1.0\r\n"
             "Content-Type: multipart/mixed; boundary=\"boundary123\"\r\n\r\n"
             "--boundary123\r\n"
             "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
             "\r\n"
             "Attached is the file '%s'.\r\n\r\n"
             "--boundary123\r\n"
             "Content-Type: application/octet-stream; name=\"%s\"\r\n"
             "Content-Transfer-Encoding: base64\r\n"
             "Content-Disposition: attachment; filename=\"%s\"\r\n"
             "\r\n"
             "%s\r\n"
             "--boundary123--\r\n",
             email, subject ? subject : "No Subject", filename, filename, filename, encoded_file);

    SSL_write(ssl, email_body, strlen(email_body));

    send_command(ssl, "\r\n.\r\n", buffer, sizeof(buffer));

    send_command(ssl, "QUIT\r\n", buffer, sizeof(buffer));

    free(encoded_file);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sockfd);
    printf("Email sent successfully to %s\n", email);
}
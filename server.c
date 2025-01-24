#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdbool.h>

#pragma comment(lib, "ws2_32.lib")

struct Node {
    char ch;
    unsigned freq;
    struct Node *left, *right;
};

struct Node* createNode(char ch, unsigned freq);
void printCodes(struct Node* root, int arr[], int top, char codes[256][256]);
void HuffmanCodes(char data[], int freq[], int size, char codes[256][256]);
void handle_compress(int client_sock, char *buffer);
void handle_download(int client_sock);
void writeCompressedFile(const char* filename, const char* output, int output_size);
void sendResponse(int client_sock, const char* status, const char* body, bool isBinary);
char* extractFileData(char *buffer, int *fileSize);
char* compressData(const char* data, int dataSize, char codes[256][256], int* compressedSize);

// Global variable to store compressed file path
const char* compressedFilePath = "compressed_output.bin";

int main() {
    WSADATA wsaData;
    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Server running on port 8080...\n");

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
            continue; // Skip this iteration
        }

        char buffer[2048] = {0};
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            fprintf(stderr, "Receive failed: %d\n", WSAGetLastError());
            closesocket(client_sock);
            continue;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the buffer

        if (strstr(buffer, "POST /compress") != NULL) {
            handle_compress(client_sock, buffer);
        } else if (strstr(buffer, "GET /download") != NULL) {
            handle_download(client_sock);
        } else {
            sendResponse(client_sock, "HTTP/1.1 404 Not Found", "Not Found", false);
        }

        closesocket(client_sock);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}

struct Node* createNode(char ch, unsigned freq) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->left = node->right = NULL;
    node->ch = ch;
    node->freq = freq;
    return node;
}

struct MinHeap {
    int size;
    int capacity;
    struct Node** array;
};

struct MinHeap* createMinHeap(int capacity) {
    struct MinHeap* minHeap = (struct MinHeap*)malloc(sizeof(struct MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = (struct Node**)malloc(minHeap->capacity * sizeof(struct Node*));
    return minHeap;
}

void swapNode(struct Node** a, struct Node** b) {
    struct Node* t = *a;
    *a = *b;
    *b = t;
}

void minHeapify(struct MinHeap* minHeap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size && minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

bool isSizeOne(struct MinHeap* minHeap) {
    return (minHeap->size == 1);
}

struct Node* extractMin(struct MinHeap* minHeap) {
    struct Node* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    minHeapify(minHeap, 0);
    return temp;
}

void insertMinHeap(struct MinHeap* minHeap, struct Node* minNode) {
    ++minHeap->size;
    int i = minHeap->size - 1;

    while (i && minNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = minNode;
}

void generateCodes(struct Node* root, char* code, int top, char codes[256][256]) {
    if (root->left) {
        code[top] = '0';
        generateCodes(root->left, code, top + 1, codes);
    }

    if (root->right) {
        code[top] = '1';
        generateCodes(root->right, code, top + 1, codes);
    }

    if (!(root->left) && !(root->right)) {
        code[top] = '\0'; // null-terminate the string
        strcpy(codes[(unsigned char)root->ch], code);
    }
}

void buildHuffmanTree(char data[], int freq[], int size, char codes[256][256]) {
    struct Node *left, *right, *top;
    struct MinHeap* minHeap = createMinHeap(size);

    for (int i = 0; i < size; ++i) {
        minHeap->array[i] = createNode(data[i], freq[i]);
    }
    minHeap->size = size;

    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);

        top = createNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;

        insertMinHeap(minHeap, top);
    }

    char code[256]; // Temporary array to hold the current code
    generateCodes(minHeap->array[0], code, 0, codes); // Generate codes
}

void HuffmanCodes(char data[], int freq[], int size, char codes[256][256]) {
    buildHuffmanTree(data, freq, size, codes);
}

// Compress the data using the generated Huffman codes
char* compressData(const char* data, int dataSize, char codes[256][256], int* compressedSize) {
    char* compressedOutput = (char*)malloc(dataSize * 8); // Max size for worst-case scenario
    int bitIndex = 0;

    for (int i = 0; i < dataSize; i++) {
        char* code = codes[(unsigned char)data[i]];
        for (int j = 0; code[j] != '\0'; j++) {
            compressedOutput[bitIndex++] = code[j];
        }
    }
    compressedOutput[bitIndex] = '\0'; // Null-terminate the output string

    *compressedSize = (bitIndex + 7) / 8; // Calculate size in bytes
    return compressedOutput;
}

// This function handles the compress request
void handle_compress(int client_sock, char *buffer) {
    int fileSize;
    char *fileData = extractFileData(buffer, &fileSize);

    if (fileData == NULL || fileSize <= 0) {
        sendResponse(client_sock, "HTTP/1.1 400 Bad Request", "Failed to extract file data.", false);
        return;
    }

    int freq[256] = {0}; // Frequency array for characters
    for (int i = 0; i < fileSize; i++) {
        freq[(unsigned char)fileData[i]]++;
    }

    char data[256]; // Unique characters
    int size = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            data[size++] = (char)i;
        }
    }

    char codes[256][256] = {0}; // Array to hold the codes for each character
    HuffmanCodes(data, freq, size, codes); // Generate Huffman codes

    int compressedSize;
    char* compressedOutput = compressData(fileData, fileSize, codes, &compressedSize); // Compress the data

    // Write compressed output to a file
    writeCompressedFile(compressedFilePath, compressedOutput, compressedSize);

    // Send a response indicating success
    sendResponse(client_sock, "HTTP/1.1 200 OK", "File compressed successfully.", false);

    free(fileData);
    free(compressedOutput);
}

// Extracts file data from the request buffer
char* extractFileData(char *buffer, int *fileSize) {
    char *dataStart = strstr(buffer, "\r\n\r\n");
    if (!dataStart) return NULL;

    dataStart += 4; // Skip the headers
    *fileSize = strlen(dataStart);
    char *fileData = (char*)malloc(*fileSize + 1);
    memcpy(fileData, dataStart, *fileSize);
    fileData[*fileSize] = '\0'; // Null-terminate the string
    return fileData;
}

// Writes the compressed output to a file
void writeCompressedFile(const char* filename, const char* output, int output_size) {
    FILE *file = fopen(filename, "wb");
    if (file) {
        fwrite(output, sizeof(char), output_size, file);
        fclose(file);
    }
}

// Send HTTP response
void sendResponse(int client_sock, const char* status, const char* body, bool isBinary) {
    char response[2048];
    if (isBinary) {
        snprintf(response, sizeof(response), "%s\r\nContent-Type: application/octet-stream\r\n\r\n%s", status, body);
    } else {
        snprintf(response, sizeof(response), "%s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s", status, strlen(body), body);
    }
    send(client_sock, response, strlen(response), 0);
}

// Handle download request (this will be expanded later)
void handle_download(int client_sock) {
    // Placeholder for download implementation
    sendResponse(client_sock, "HTTP/1.1 200 OK", "Download functionality not yet implemented.", false);
}

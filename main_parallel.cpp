int encryptFile(const char* inputFile, const char* outputFile, unsigned char* expandedKey) {
    int fd_in = open(inputFile, O_RDONLY);
    if (fd_in < 0) {
        const char* err = "Error opening input file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        return 1;
    }

    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        const char* err = "Error getting file size\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        close(fd_in);
        return 1;
    }

    size_t fileSize = st.st_size;
    size_t paddedSize = (fileSize % 16 == 0) ? fileSize : ((fileSize / 16) + 1) * 16;

    unsigned char header[16] = {0};
    memcpy(header, &fileSize, sizeof(fileSize));

    unsigned char* buffer = new unsigned char[paddedSize]();
    unsigned char* encryptedBuffer = new unsigned char[paddedSize];

    AESEncrypt(header, expandedKey, encryptedBuffer);

    ssize_t bytesRead = read(fd_in, buffer, fileSize);
    if (bytesRead < 0) {
        const char* err = "Error reading file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_in);
        return 1;
    }
    close(fd_in);

    for (size_t i = 0; i < paddedSize; i += 16) {
        AESEncrypt(buffer + i, expandedKey, encryptedBuffer + 16 + i);
    }

    int fd_out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        const char* err = "Error creating output file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] encryptedBuffer;
        return 1;
    }

    if (write(fd_out, encryptedBuffer, paddedSize) < 0) {
        const char* err = "Error writing to output file\n";
        if (write(STDERR_FILENO, err, strlen(err)) < 0) return 1;
        delete[] buffer;
        delete[] encryptedBuffer;
        close(fd_out);
        return 1;
    }

    close(fd_out);
    delete[] buffer;
    delete[] encryptedBuffer;
    return 0;
}
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <math.h>

using namespace std;

static size_t ZERO = 0;

class BinaryMmap {

private:

    int fd;
    int pageSize = getpagesize();
    struct stat s;
    size_t position = 0;
    size_t totalSize = 0;
    int memPagesCount;

public:
    char *map;
    BinaryMmap(const string &path, int memPagesCount = 250) {
        this->memPagesCount = memPagesCount;

        fd = open(path.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        fstat(fd, &s);

        position = s.st_size;
        totalSize = s.st_size + (pageSize - s.st_size % pageSize);
        totalSize += pageSize * memPagesCount;

        ftruncate(fd, totalSize);
        fsync(fd);

        map = (char *)mmap(0, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }

    ~BinaryMmap() {
        terminate();
    }

    // Write

    template <class String>
    void writeStr(const String str, size_t &at = ZERO, int maxLenBytes = 1) {
        size_t &writeAt = (at == 0) ? position : at;

        int maxLen = max(2, (int)pow(2, maxLenBytes * 8)) - 1;
        int bound = min(str.length(), (size_t)maxLen);

        if (bound > remainingSpace())
            increaseSize();

        for (size_t i = 0; i < bound; i++) {
            map[writeAt++] = str[i];
            if (writeAt > position)
                position = writeAt;
        }
    }

    template <class Int>
    void writeInt(Int a, int onBytes = 4, size_t &at = ZERO) {
        size_t &writeAt = (at == 0) ? position : at;

        if (onBytes > remainingSpace())
            increaseSize();

        for (int i = onBytes - 1; i >= 0; --i) {
            uint8_t byte = a & 0xFF;
            map[writeAt + i] = byte;
            a >>= 8;
        }

        if (writeAt + onBytes > position)
            position = writeAt + onBytes;
    }

    template <class Collection, class ColIterator>
    void writeCollection(const Collection &c, int clusterSize = 2) {
        for (ColIterator it = c.begin(); it != c.end(); ++it) {
            writeInt(*it, clusterSize);
        }
    }

    // Read

    string_view readStr(size_t &location, int length) {
        char *start = map + location;
        return string_view(start, length);
    }

    size_t readInt(size_t &location, int length = 4) {
        size_t result = 0;
        for (int n = location; n < length + location; ++n)
            result = (result << 8) + (uint8_t)map[n];
        return result;
    }

    size_t currentPosition() {
        return position;
    }

    void terminate() {
        ftruncate(fd, position);
        fsync(fd);
        msync(map, position, MS_SYNC);
        munmap(map, totalSize);
        close(fd);
    }

private:

    void increaseSize() {
        msync(map, totalSize, MS_SYNC);
        munmap(map, totalSize);

        totalSize += memPagesCount * pageSize;
        ftruncate(fd, totalSize);
        fsync(fd);

        map = (char *)mmap(0, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }

    size_t remainingSpace() {
        return totalSize - position;
    }

};
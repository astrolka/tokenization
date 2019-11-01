#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <math.h>
#include <map>

using namespace std;

static size_t ZERO = 0;
static map<int, size_t> maxUInts;

class BinaryMmap {

private:

    int fd;
    int pageSize = getpagesize();
    struct stat s;
    size_t position = 0;
    size_t usedBytes = 0;
    size_t totalSize = 0;
    int memPagesCount;

public:
    char *map;
    BinaryMmap(const string &path, int memPagesCount = 250) {
        this->memPagesCount = memPagesCount;

        fd = open(path.c_str(), O_RDWR | O_CREAT, (mode_t)0600);
        fstat(fd, &s);

        usedBytes = position = s.st_size;
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
    void writeStr(const String str) {
        writeStr(str, position, 1);
    }

    template <class String>
    void writeStr(const String str, size_t &at) {
        writeStr(str, at, 1);
    }

    template <class String>
    void writeStr(const String str, size_t &at, int maxLenBytes) {
        position = at;

        size_t bound = min(str.length(), maxUnsignedInt(maxLenBytes));

        while (position + bound > totalSize)
            increaseSize();

        for (size_t i = 0; i < bound; i++)
            map[position++] = str[i];

        if (position > usedBytes)
            usedBytes = position;
    }

    template <class Int>
    void writeInt(Int a) {
        writeInt(a, 4, position);
    }

    template <class Int>
    void writeInt(Int a, int onBytes) {
        writeInt(a, onBytes, position);
    }

    template <class Int>
    void writeInt(Int a, int onBytes, size_t &at) {
        position = at;

        while (position + onBytes > totalSize)
            increaseSize();

        a = min((size_t)a, maxUnsignedInt(onBytes));

        for (int i = onBytes - 1; i >= 0; --i) {
            uint8_t byte = a & 0xFF;
            map[position + i] = byte;
            a >>= 8;
        }
        position += onBytes;

        if (position > usedBytes)
            usedBytes = position;
    }

    template <class Collection, class ColIterator>
    void writeCollection(const Collection &c, int clusterSize = 2) {
        for (ColIterator it = c.begin(); it != c.end(); ++it) {
            writeInt(*it, clusterSize);
        }
    }

    // Read

    string_view readStr(int length) {
        return readStr(length, position);
    }

    string_view readStr(int length, size_t &location) {
        char *start = map + location;
        position = location + length;
        return string_view(start, length);
    }

    size_t readInt(int length) {
        return readInt(length, position);
    }

    size_t readInt(int length, size_t &location) {
        size_t result = 0;
        for (int n = location; n < length + location; ++n)
            result = (result << 8) + (uint8_t)map[n];
        position = location + length;
        return result;
    }

    size_t currentPosition() {
        return position;
    }

    void updateCurrentPosition(size_t newPosition = 0) {
        while (newPosition > totalSize)
            increaseSize();
        position = newPosition;
    }

    size_t writtenBytes() {
        return usedBytes;
    }

    void terminate() {
        ftruncate(fd, usedBytes);
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
        return totalSize - usedBytes;
    }

    size_t maxUnsignedInt(int bytes) {
        if (maxUInts.count(bytes)) {
            return maxUInts[bytes];
        }
        size_t mi = max((long double)2, (long double)powl(2, bytes * 8)) + 0.5;
        mi--;
        maxUInts.insert(pair<int, size_t>(bytes, mi));
        return mi;
    }

};
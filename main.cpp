#include <iostream>
#include <sys/stat.h>
#include "BinaryMmap.cpp"
#include <string>
#include <string_view>
#include <regex>
#include <fstream>
#include <chrono>

using namespace std;
using namespace std::chrono;

int main() {

    string path;
    struct stat s;
    size_t size;
    size_t pageSize = getpagesize();

    BinaryMmap tokens("tokens.bin");
    ofstream directIndex("directIndex.txt", ios_base::app);

    cout << "Enter path to file with articles" << endl;
    cin >> path;

    int fd = open(path.c_str(), O_RDONLY);
    fstat(fd, &s);

    size = s.st_size;
    size += pageSize - size % pageSize;

    char *c = (char *) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    string_view text(c);

    size_t position = 0;
    const regex boundaryRX("wiki_search_engine_38hf91\\|title=(.+)\\|pageId=(\\d+)\\|docId=(\\d+)\\|size=(\\d+)");
    const regex wordRX("(?:\\w|\\xD0[\\x80-\\xBF]|\\xD1[\\x80-\\x9F]|\\xCC[\\x80-\\xBB])+");

    high_resolution_clock::time_point totalTimer = high_resolution_clock::now();
    high_resolution_clock::time_point kbTimer = high_resolution_clock::now();

    int bytesOfText = 0;
    int kbCount = 0;
    long long kbTime = 0;

    size_t tokensCount = 0;
    double tokenLength = 0;

    while (true) {
        size_t boundEnd = text.find('\n', position);
        if (boundEnd > text.length())
            break;
        string boundary(text.substr(position, boundEnd - position));
        position = boundEnd + 1;

        smatch boundMatch;
        regex_match(boundary, boundMatch, boundaryRX);

        string articleTitle = boundMatch.str(1);
        int pageId = atoi(boundMatch.str(2).c_str());
        int docId = atoi(boundMatch.str(3).c_str());
        int articleSize = atoi(boundMatch.str(4).c_str());

        auto beginIt = text.begin() + position;
        auto endIt = text.begin() + position + articleSize;
        match_results<string_view::const_iterator> wordM;

        size_t tokenLocation = 0;

        while (regex_search(beginIt, endIt, wordM, wordRX)) {
            size_t size = wordM[0].second - wordM[0].first;
            string_view word(&*wordM[0].first, size);

            tokens.writeInt(word.length(), 1);
            tokens.writeStr(word);
            tokens.writeInt(tokenLocation, 3);
            tokens.writeInt(docId, 2);

            tokenLocation += 1;
            beginIt += wordM.position() + size;

            bytesOfText += wordM.position() + size;
            if (bytesOfText >= 1024) {
                high_resolution_clock::time_point now = high_resolution_clock::now();
                kbTime = (kbTime * kbCount + duration_cast<microseconds>(now - kbTimer).count()) / (kbCount + 1);
                kbTimer = now;
                kbCount++;
                bytesOfText = 0;
            }

            tokenLength = (tokensCount * tokenLength + size) / (tokensCount + 1);
            tokensCount++;
        }

        directIndex << "docId=" << docId << "|title=" << articleTitle << "|pageId=" << pageId << endl;

        position += articleSize + 1;
    }

    high_resolution_clock::time_point now = high_resolution_clock::now();

    cout << "total time in milliseconds:\t" << duration_cast<milliseconds>(now - totalTimer).count() << endl;
    cout << "time on 1 kb of text in microseconds:\t" << kbTime << endl;
    cout << "tokens count:\t" << tokensCount << endl;
    cout << "tokens length:\t" << tokenLength << endl;

    directIndex.close();
    close(fd);
    munmap(c, size);
    tokens.terminate();

    return 0;
}
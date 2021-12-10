#pragma once

#include <fts.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDebug>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ExtentInfo {
public:
    struct FileNode {
        std::string path;
        std::size_t size;
        std::size_t extCnt;
        std::vector<std::pair<uint64_t, uint64_t>> exts;
        FileNode() = default;
        FileNode(std::string path, std::size_t size, std::size_t extCnt) : path(path), size(size), extCnt(extCnt) {}
        FileNode(const FileNode &f) = default;
    };

    struct ExtNode {
        uint64_t start;
        uint64_t end;
        std::size_t fileIndex;
        ExtNode(uint64_t end) : start(0), end(end), fileIndex(0) {}
        ExtNode(uint64_t start, uint64_t end, std::size_t fileIndex) : start(start), end(end), fileIndex(fileIndex) {}
        bool operator<(const ExtNode &b) const { return end < b.end; }
    };

    std::size_t getExtCountFromBlockRange(std::size_t start, std::size_t end) {
        auto left = extents.upper_bound(start);
        auto right = extents.upper_bound(end);
        if (left == extents.end()) {
            return 0;
        }
        if (right == extents.end()) {
            if (left->end > start) {
                return 1;
            }
            return std::distance(left, --extents.end());
        }
        if (left == right && (*left).start < end) {
            return 1;
        }
        return std::distance(left, right);
    }

    std::vector<FileNode> getFileListFromBlockRange(std::size_t start, std::size_t end) {
        std::vector<FileNode> ret;
        auto iter = extents.upper_bound(start);
        for (; iter != extents.upper_bound(end); ++iter) {
            ret.push_back(fileExtentInfo.at((*iter).fileIndex));
        }
        if (iter != extents.end() && (*iter).start < end) {
            ret.push_back(fileExtentInfo.at((*iter).fileIndex));
        }
        return ret;
    }

    std::vector<FileNode> getFileListSortByExtCount(std::size_t num) {
        if (sortedFileExtentInfo.size() < num) {
            num = sortedFileExtentInfo.size();
        }
        auto sortMethed = [](const FileNode &f1, const FileNode &f2) {
            if (f1.extCnt == f2.extCnt) {
                return f1.size > f2.size;
            }
            return f1.extCnt > f2.extCnt;
        };
        std::nth_element(sortedFileExtentInfo.begin(), sortedFileExtentInfo.begin() + num, sortedFileExtentInfo.end(),
                         sortMethed);
        std::vector<FileNode> ret(sortedFileExtentInfo.begin(), sortedFileExtentInfo.begin() + num);
        std::sort(ret.begin(), ret.end(), sortMethed);
        return ret;
    }

    std::vector<FileNode> getFileListSortByFileSize(std::size_t num) {
        if (sortedFileExtentInfo.size() < num) {
            num = sortedFileExtentInfo.size();
        }
        auto sortMethed = [](const FileNode &f1, const FileNode &f2) {
            if (f1.size == f2.size) {
                return f1.extCnt > f2.extCnt;
            }
            return f1.size > f2.size;
        };
        std::nth_element(sortedFileExtentInfo.begin(), sortedFileExtentInfo.begin() + num, sortedFileExtentInfo.end(),
                         sortMethed);
        std::vector<FileNode> ret(sortedFileExtentInfo.begin(), sortedFileExtentInfo.begin() + num);
        std::sort(ret.begin(), ret.end(), sortMethed);
        return ret;
    }

    FileNode getFileInfoByPath(std::string path) { return fileExtentInfo.at(pathIndex.at(path)); }

    FileNode getFileInfoByIndex(std::size_t index) { return fileExtentInfo.at(index); }

    void getFileExtentInfo(const char *path) {
        int fd = open(path, O_RDONLY | O_LARGEFILE);

        struct stat buf;
        int ret = fstat(fd, &buf);
        if (ret < 0) {
            close(fd);
            return;
        }
        size_t size = buf.st_size;

        char fiemap_buffer[16 * 1024];
        struct fiemap *fiemap = (struct fiemap *)fiemap_buffer;
        memset(fiemap, 0, sizeof(struct fiemap));
        int max_count = (sizeof(fiemap_buffer) - sizeof(struct fiemap)) / sizeof(struct fiemap_extent);
        fiemap->fm_extent_count = max_count;
        fiemap->fm_length = ~0ULL;
        ioctl(fd, FS_IOC_FIEMAP, fiemap);

        uint32_t extCnt = fiemap->fm_mapped_extents;
        if (extCnt > 0) {
            FileNode fileNode(path, size, extCnt);
            for (uint32_t i = 0; i < extCnt; i++) {
                struct fiemap_extent *ext = fiemap->fm_extents + i;
                uint64_t physical_start = ext->fe_physical;
                uint64_t physical_end = ext->fe_physical + ext->fe_length - 1;

                if (!fileNode.exts.empty() && physical_start / 4096 == fileNode.exts.back().second) {
                    fileNode.exts.back().second = physical_end / 4096;
                } else {
                    fileNode.exts.emplace_back(physical_start / 4096, physical_end / 4096);
                }
            }
            fileExtentInfo.push_back(fileNode);

            std::size_t index = fileExtentInfo.size() - 1;
            pathIndex.emplace(path, index);
            for (const auto &ext : fileNode.exts) {
                extents.emplace(ext.first, ext.second, index);
            }
        }

        close(fd);
    }

protected:
    void scan(std::string path) {
        cleanup();

        char *buf[2];
        char dir[256];
        strcpy(dir, path.c_str());
        buf[0] = dir;
        buf[1] = nullptr;

        FTS *fts = fts_open(buf, FTS_PHYSICAL | FTS_XDEV | FTS_NOCHDIR | FTS_NOSTAT, nullptr);
        while (1) {
            FTSENT *ent = fts_read(fts);
            if (!ent)
                break;
            switch (ent->fts_info) {
            case FTS_NSOK:
            case FTS_NS:
            case FTS_D:
            case FTS_F:
                getFileExtentInfo(ent->fts_path);
            }
        }
        fts_close(fts);

        sortedFileExtentInfo = fileExtentInfo;
    }

private:
    void cleanup() {
        std::vector<FileNode>().swap(fileExtentInfo);
        std::vector<FileNode>().swap(sortedFileExtentInfo);
        pathIndex.clear();
        extents.clear();
    }

private:
    // all file info
    std::vector<FileNode> fileExtentInfo;
    // for sort by <size> or <extCnt>
    std::vector<FileNode> sortedFileExtentInfo;
    // for search by <path>
    std::unordered_map<std::string, std::size_t> pathIndex;
    // all extents info, sorted by <end>
    std::set<ExtNode> extents;
};

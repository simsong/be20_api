#ifndef ABSTRACT_IMAGE_READER
#define  ABSTRACT_IMAGE_READER

#include <string>
#include <cstdint>

class abstract_image_reader {
public:
    abstract_image_reader() {};
    virtual ~abstract_image_reader();
    virtual ssize_t pread(void *buf, size_t bufsize, uint64_t offset) const = 0;
    virtual int64_t image_size() const=0;
    virtual const std::string image_fname() const = 0;
};

#endif

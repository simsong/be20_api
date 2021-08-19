#ifndef ABSTRACT_IMAGE_READER
#define  ABSTRACT_IMAGE_READER

class abstract_image_reader {
    abstract_image_reader() {};
    virtual ~abstract_image_reader() = 0;
public:
    virtual ssize_t pread(char *buf, size_t bufsize, uint64_t offset) = 0;
    virtual int64_t image_size() const=0;
    virtual const std::string &image_fname() const = 0;
};

#endif

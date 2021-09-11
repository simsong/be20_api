#ifndef DISTINCT_CHARACTER_COUNTER_H
#define DISTINCT_CHARACTER_COUNTER_H

class character_counter {
public:;
    class underflow : public std::exception {
        const char *error {nullptr};
    public:
        underflow(const char *error_):error(error_) {}
        const char* what() const noexcept override { return error; }
    };
};


class highbit_character_counter : public character_counter {
public:
    highbit_character_counter(){};
    int highbit_count {0};

    void preload(const uint8_t *buf, size_t ct) {
        for (size_t i = 0; i<ct; i++){
            add(buf[i]);
        }
    }

    void add(uint8_t ch) {
        if (ch & 0x80){
            highbit_count += 1;
        }

    }

    void remove(uint8_t ch) {
        if (ch & 0x80) {
            if (highbit_count==0){
                throw underflow("highbit_character_counter: underflow");
            }
            highbit_count -= 1;
        }
    }
};


class distinct_character_counter : public character_counter {
    size_t count[256];             // how many times we have seen this character
public:
    distinct_character_counter() {
        memset(count,0,sizeof(count));
    };

    int distinct_count {0};

    void preload(const uint8_t *buf, size_t ct) {
        for (size_t i = 0; i<ct; i++){
            add(buf[i]);
        }
    }

    void add(uint8_t ch) {
        count[ch] += 1;
        if (count[ch]==1) {
            distinct_count += 1;
        }
    }

    void remove(uint8_t ch) {
        if (count[ch]==0){
            throw underflow("distinct_character_counter: underflow");
        }
        count[ch] -= 1;
        if (count[ch]==0) {
            distinct_count -= 1;
        }
    }

};

#endif

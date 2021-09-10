#ifndef DISTINCT_CHARACTER_COUNTER_H
#define DISTINCT_CHARACTER_COUNTER_H

class distinct_character_counter {
    size_t count[256];             // how many times we have seen this character
public:
    distinct_character_counter() {
        memset(count,0,sizeof(count));
    };

    class underflow : public std::exception {
    public:
        underflow() {}
        const char* what() const noexcept override { return "distinct_character_counter::underflow"; }
    };

    int distinct_count {0};
    void add(uint8_t ch) {
        count[ch] += 1;
        if (count[ch]==1) {
            distinct_count += 1;
        }
    }

    void remove(uint8_t ch) {
        if (count[ch]==0) throw underflow();
        count[ch] -= 1;
        if (count[ch]==0) {
            distinct_count -= 1;
        }
    }

};

#endif

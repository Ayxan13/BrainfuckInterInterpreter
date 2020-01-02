/*
 * 01 Jan 2020
 * BrainFuck interpreter.
 * Written by Ayxan Haqverdili
 */

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stack>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <ciso646>  // and/or/not
#endif              // !_MSC_VER

#ifdef __GNUC__
# define pure_attribute [[gnu::pure]]
# define const_attribute [[gnu::const]]
#else
# define pure_attribut
# define const_attribute
#endif // __GNUC__

/* "Infinite" unsigned char buffer pointer */
class Pointer {
public:
    using storage_type = std::deque<char>;
    using size_type = storage_type::size_type;

    explicit Pointer(size_type const preAllocatedMemory = 1)
            : mem_(preAllocatedMemory), index_{ 0 } {
        /* preAllocatedMemory must be at least 1 */
        assert(preAllocatedMemory != 0);
    }

    auto& operator+=(size_type const c) {
        index_ += c;
        /* Allocate memory if needed. */
        if (mem_.size() <= index_) mem_.resize(index_ + 1);
        return *this;
    }

    auto operator++() -> Pointer& { return (*this += 1); }

    auto& operator-=(size_type const c) noexcept {
        assert(index_ - c < index_);
        index_ -= c;
        return *this;
    }

    auto operator--() noexcept -> Pointer& { return (*this -= 1); }

    auto operator++(int) const->Pointer = delete; /* Expensive and pointless. Use preincrement instead */
    auto operator--(int) const->Pointer = delete; /* Expensive and pointless. Use predecrement instead */

    [[nodiscard]] auto operator*() const& -> const storage_type::value_type& {
        assert(mem_.size() > index_);
        return mem_[index_];
    }

    [[nodiscard]] auto operator*() & -> storage_type::value_type& {
        return const_cast<storage_type::value_type&>(*std::as_const(*this));
    }

private:
    [[no_unique_address]] storage_type mem_;
    [[no_unique_address]] size_type index_;
};

/* A class that contains one of "><+-.,[]" and how many times it is supposed to be executed consecutively */
class Command {
public:
    Command(char const ch, std::size_t const sz) : command_{ ch }, count_{ sz } {}
    [[nodiscard]] auto command() const noexcept { return command_; }
    [[nodiscard]] auto count() const noexcept { return count_; }

    enum ActionableCommands : char {
        PointerIncr = '>',
        PointerDecr = '<',
        CellValIncr = '+',
        CellValDecr = '-',
        Cout = '.',
        Cin = ',',
        LoopBegin = '[',
        LoopEnd = ']',
    };
private:
    char command_;
    std::size_t count_;
};

const_attribute [[nodiscard]] bool operator==(Command const com, char const ch) noexcept {
    return com.command() == ch;
}

const_attribute [[nodiscard]] bool operator!=(Command const com, char const ch) noexcept {
    return not (com == ch);
}

template <char op, typename T1, typename T2> // Workaround for "-Wconversion" being buggy
inline static void operation(T1& t, T2 const t2) noexcept {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif // __GNUC__

    if constexpr (op == '+') t += static_cast<T1>(t2);
    else if constexpr (op == '-') t -= static_cast<T1>(t2);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC__
}

/* Return false if `ch` is a comment, true if it is a command() */
bool interpret(Command const com, Pointer& p) {
    auto const ch = com.command();
    auto const count = com.count();
    switch (ch) {
        case Command::PointerIncr: {
            p += count;
            break;
        }
        case Command::PointerDecr: {
            p -= count;
            break;
        }
        case Command::CellValIncr: {
            operation<'+'>(*p, count);
            break;
        }
        case Command::CellValDecr: {
            operation<'-'>(*p, count);
            break;
        }
        case Command::Cout: {
            std::fill_n(std::ostream_iterator<unsigned char>(std::cout), count, *p);
            break;
        }
        case Command::Cin: {
            for (std::size_t i = 0; i != count; ++i) {
                std::cin >> *p;
            }
            break;
        }
        default: /* Everything else is a comment */
            return false;
    }
    return true;
}

/* Skips from `[` to the corresponding `]` taking account nested loops. */
template <typename BidirIter>
[[nodiscard]] auto skipLoop(BidirIter p) noexcept {
    assert(*p == Command::LoopBegin);
    std::size_t bracketCount = 0;
    do {
        if (*p == Command::LoopBegin)
            ++bracketCount;
        else if (*p == Command::LoopEnd)
            --bracketCount;
        ++p;
    } while (bracketCount != 0);
    return p - 1;
}

/* Is `[` or `]` */
const_attribute [[nodiscard]] auto isLoopCommand(char const ch) noexcept -> bool {
    switch (ch) {
        case Command::LoopBegin:
        case Command::LoopEnd:
            return true;
        default:
            return false;
    }
}

/* Is one of the actionable 8 characters */
const_attribute [[nodiscard]] auto isCommand(char const ch) noexcept -> bool {
    switch (ch) {
        case Command::LoopBegin:
        case Command::LoopEnd:
        case Command::PointerIncr:
        case Command::PointerDecr:
        case Command::CellValIncr:
        case Command::CellValDecr:
        case Command::Cout:
        case Command::Cin:
            return true;
        default:
            return false;
    }
}


/* Skip all characters until for one `isCommand` returns true. */
template <typename InputIter>
[[ nodiscard ]] auto skipComment(InputIter beg, InputIter const end) {
    while (beg != end and not isCommand(*beg)) ++beg;
    return beg;
}

template <typename InputIter>
[[nodiscard]] auto generateSourceCode(InputIter beg, InputIter const end) {
    std::vector<Command> source_code;
    while (beg != end) {
        auto const ch = *beg;
        std::size_t count = 0;
        if (isLoopCommand(ch)) { // Executing more than 1 loop command doesn't work.
            ++count;
            ++beg;
        }
        else if (isCommand(ch)) { // Accumulate commands
            while ((beg = skipComment(beg, end)) != end and *beg == ch) {
                ++count;
                ++beg;
            }
        }
        else {
            /* Everything else is a comment and is ignored. */
            ++beg;
        }
        source_code.emplace_back(ch, count);
    }
    return source_code;
}

int main(int const argc, char* const argv[]) {
    Pointer p;
    if (argc < 2) {
        std::cerr << "Source-code file name needed\n";
        return EXIT_FAILURE;
    }
    std::ifstream f{ argv[1] };
    if (not f.is_open()) {
        std::cerr << "Can't open the source-code file \n";
        return EXIT_FAILURE;
    }
    using StremIter = std::istream_iterator<char>;
    const auto sourceCode = generateSourceCode(StremIter{ f }, StremIter{});

    auto it = sourceCode.cbegin();
    auto const end = sourceCode.cend();

    std::stack<decltype(it)> loopPos; /* Here we log loops */
    while (it != end) {
        /* Firstly we consider loops  */
        if (*it == Command::LoopBegin) {
            /* If the current cell is zero, skip the loop. */
            if (*p == 0) {
                it = skipLoop(it);
            }
            else /* Else, log the loop starting */
            {
                loopPos.push(it);
            }
            ++it;
        }
        else if (*it == Command::LoopEnd) /* Jump to the last `]` */
        {
            assert(not loopPos.empty());
            it = loopPos.top();
            loopPos.pop();
            /* don't increment `it` */
        }
        else {
            interpret(*it, p);
            ++it;
        }
    }
}

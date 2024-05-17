#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }
    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    ~Vector() {
        if (size_ != 0) {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        // constexpr оператор if будет вычислен во время компиляции
        UninitializedData(other.data_, data_);
    }

    Vector(Vector&& other) noexcept
        : data_(std::exchange(other.data_, RawMemory<T>()))
        , size_(std::exchange(other.size_, 0)) {
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые или удалив существующие */
                //size_t count = std::abs(rhs.size_ - size_);
                size_t count = (rhs.size_ < size_) ? size_ - rhs.size_ : rhs.size_ - size_;                
                if (rhs.size_ < size_) {
					std::copy_n(rhs.data_.GetAddress(), rhs.size_, begin());
                    std::destroy_n(data_.GetAddress() + rhs.size_, count);
                }
                else {
                    std::copy_n(rhs.data_.GetAddress(), size_, begin());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, count, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::exchange(rhs.data_, RawMemory<T>());
            size_ = std::exchange(rhs.size_, 0);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // Конструируем элементы в new_data, копируя их из data_
        // constexpr оператор if будет вычислен во время компиляции        
        UninitializedNewData(new_data);

        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    void Resize(size_t new_size) {
        if (new_size > data_.Capacity()) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress(), new_size - size_);
        }
        else
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T>(value));
    }

    void PopBack() noexcept {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {

        size_t pos_idx = std::distance(cbegin(), pos);

        if (size_ == Capacity()) {
            InsertWithAlloc(pos, pos_idx, std::forward<Args>(args)...);
        }
        else 
        {   //size < capacity
            InsertWithoutAlloc(pos_idx, std::forward<Args>(args)...);
        }

        ++size_;
        return begin() + pos_idx;
    }

    iterator Erase(const_iterator pos) {
        size_t pos_idx = std::distance(cbegin(), pos);
        std::move(begin() + pos_idx + 1, end(), begin() + pos_idx);
        std::destroy_n(end() - 1, 1);
        --size_;
        return begin() + pos_idx;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T * buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T * buf, const T & elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T * buf) noexcept {
        buf->~T();
    }

    void UninitializedData(const RawMemory<T>& from, RawMemory<T>& to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from.GetAddress(), size_, to.GetAddress());
        }
        else {
            std::uninitialized_copy_n(from.GetAddress(), size_, to.GetAddress());
        }
    }
    void UninitializedNewData(RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
    }

    template <typename... Args>
    void InsertWithoutAlloc(size_t pos_idx, Args&&... args) {
        if (size_ == pos_idx) {
            new (begin() + pos_idx) T(std::forward<Args>(args)...);
        }
        else
        {
            auto tmp = T(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(*(end() - 1)));
            std::move_backward(begin() + pos_idx, end() - 1, end());
            data_[pos_idx] = std::move(tmp);
        }
    }

    template <typename... Args>
    void InsertWithAlloc(const_iterator pos, size_t pos_idx, Args&&... args) {
        size_t count_prev = std::distance(cbegin(), pos);
        size_t count_next = std::distance(pos, cend());

        RawMemory<T> new_data((size_ == 0) ? 1 : 2 * size_);
        new (new_data.GetAddress() + pos_idx) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            try {
                std::uninitialized_move_n(begin(), count_prev, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_n(begin(), count_prev);
            }
            try {
                std::uninitialized_move_n(begin() + count_prev, count_next, new_data.GetAddress() + count_prev + 1);
            }
            catch (...) {
                std::destroy_n(begin() + count_prev, count_next);
            }
        }
        else
        {
            try {
                std::uninitialized_copy_n(begin(), count_prev, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_n(begin(), count_prev);
            }
            try {
                std::uninitialized_copy_n(begin() + count_prev, count_next, new_data.GetAddress() + count_prev + 1);
            }
            catch (...) {
                std::destroy_n(begin() + count_prev, count_next);
            }

        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
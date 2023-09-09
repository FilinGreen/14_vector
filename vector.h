#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

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
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        std::swap(buffer_, rhs.buffer_);
        std::swap(capacity_, rhs.capacity_);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
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

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

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

    //constructors
    Vector() = default;

    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)), size_(std::move(other.size_)) {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }


    //iterators
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


    //operators
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.Size(), data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + Size(), data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + Size(), rhs.Size() - Size(), data_.GetAddress());
                }
            }
        }
        size_ = rhs.size_;
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = rhs.size_;
        rhs.size_ = 0;
        return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    //functions
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t p_index = pos - begin();                             //Индекс вставки

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);      //Создает новый массив в памяти   
            new(new_data + p_index) T(std::forward<Args>(args)...); //Создаем в новом массиве по индексу pos вставляемый элемент


            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), p_index, new_data.GetAddress());                                //Перемещает элементы до pos
                std::uninitialized_move_n(data_.GetAddress() + p_index, size_ - p_index, new_data.GetAddress() + p_index + 1);//Перемещает элементы после pos
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), p_index, new_data.GetAddress());                                //Копирует элоементы до pos
                std::uninitialized_copy_n(data_.GetAddress() + p_index, size_ - p_index, new_data.GetAddress() + p_index + 1);//Копирует элементы после pos
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);

        }
        else {
            if (pos == cend()) {
                new (end()) T(std::forward<Args>(args)...);
            }
            else {
                T buffer(std::forward<Args>(args)...);                            //Создает вставляемый элемент как "буфер"
                new (data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));  //Копируем последний элемент в следующую "ячейку"
                std::move_backward(begin() + p_index, end() - 1, end());          //Перемещаем все элементы c pos на 1 вправо
                *(begin() + p_index) = std::move(buffer);                         //Перемещаем элемент "буфер" в вектор
            }
        }

        ++size_;
        return begin() + p_index;
    }


    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t p_index = pos - begin();
        for (size_t index = p_index; index < size_ - 1; ++index) {
            *(data_.GetAddress() + index) = std::move(*(data_.GetAddress() + index + 1));
        }
        PopBack();
        return begin() + p_index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    void PushBack(const T& value) {

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2); //Создает новый массив в памяти
            new (new_data + size_) T(value);                   //Создает новый обект в конце массива

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);

        }
        else {
            new (data_ + size_) T(value);
        }
        ++size_;
    }


    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);  //Создает новый массив в памяти
            new (new_data + size_) T(std::move(value));         //Создает новый обект в конце массива

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);

        }
        else {
            new (data_ + size_) T(std::move(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);                 //Создает новый массив в памяти
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);//Создает новый обект в конце массива

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());

            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);

        }
        else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        ++size_;

        return data_[size_ - 1];
    }


    void PopBack() /* noexcept */ {
        if (size_ != 0) {
            std::destroy_n(data_.GetAddress() + size_ - 1, 1);
            --size_;
        }
    }




    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        size_t buf = size_;
        size_ = other.size_;
        other.size_ = buf;
    }

    void Resize(size_t new_size) {
        if (new_size > size_) { //Увеличить размер
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {                  //Уменьшить размер и удалить лишние элементы
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }

        size_ = new_size;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }


    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }






private:
    RawMemory<T> data_;
    size_t size_ = 0;

    //private functions
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

};
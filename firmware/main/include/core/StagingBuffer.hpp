//
// Created by tgian on 26. 7. 22..
//

#ifndef FIRMWARE_STAGINGBUFFER_HPP
#define FIRMWARE_STAGINGBUFFER_HPP

#include <concepts>
#include <atomic>

namespace oasis::core {

    template <typename T>
    concept  staging_variable_trait =
            std::is_trivially_copy_assignable_v<T>
        &&  std::atomic<T>::is_always_lock_free;

    template <staging_variable_trait T>
    class StagingBuffer {
    public:
        StagingBuffer(StagingBuffer const& other) = delete;
        StagingBuffer(StagingBuffer&& other) noexcept = delete;
        StagingBuffer& operator=(StagingBuffer const& other) = delete;
        StagingBuffer& operator=(StagingBuffer&& other) noexcept = delete;

        void push(T value) {
            m_writer_value.store(value, std::memory_order_release);
            m_modified.store(true, std::memory_order_release);
        }

        bool is_modified() {

            return m_modified.load(std::memory_order_acquire);
        }
        void forward() {
            m_reader_value = m_writer_value.load(std::memory_order_acquire);
            m_modified.store(false, std::memory_order_release);
        }

        T get() {
            if (is_modified()) {
                forward();
            }
            return m_reader_value;
        }

        T get_stored() const {
            return m_reader_value;
        }

        T operator*() const {
            return m_reader_value;
        }

    private:
        std::atomic<T> m_writer_value {};
        T m_reader_value {};
        std::atomic<bool> m_modified {false};

    };


}



#endif //FIRMWARE_STAGINGBUFFER_HPP

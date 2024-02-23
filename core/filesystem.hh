#ifndef RAYBASE_FILESYSTEM_HH
#define RAYBASE_FILESYSTEM_HH
#include <string>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <filesystem>
namespace fs = std::filesystem;

namespace rb
{

class filesystem;
class file
{
friend class filesystem;
public:
    file();
    file(const file& f);
    file(file&& f) noexcept;
    ~file();

    const uint8_t* get_data() const;
    size_t get_size() const;
    const std::string& get_name() const;
    std::string get_extension() const;
    filesystem* get_source_filesystem() const;

    // Use this only when the file should be text.
    std::string get_string() const;

    template<typename... Args>
    bool read(Args&... args);
    void seek(std::uint32_t offset);
    std::uint32_t tell() const;
    bool finished() const;
    operator bool() const;

    file& operator=(const file& other);

private:
    file(
        const std::string& name,
        const uint8_t* data,
        size_t size,
        filesystem* source
    );

    std::string name;
    const uint8_t* data;
    size_t size;
    filesystem* source;
    size_t read_head;
};

class filesystem
{
friend class file;
public:
    filesystem() = default;
    filesystem(const filesystem&) = delete;
    filesystem(filesystem&&) noexcept = delete;
    virtual ~filesystem() = default;

    virtual bool exists(const std::string& name) const = 0;
    file get(const std::string& name);
    file operator[](const std::string& name);

protected:
    virtual void map(
        const std::string& name, const uint8_t*& data, size_t& size
    ) = 0;

    virtual void unmap(
        const std::string& name, const uint8_t* data, size_t size
    ) = 0;

private:
    std::recursive_mutex ref_lock;
    void ref(const std::string& name);
    void unref(const std::string& name);

    struct file_data
    {
        const uint8_t* data;
        size_t size;
        size_t refcount;
    };

    std::unordered_map<std::string, file_data> files;
};

class native_filesystem: public filesystem
{
public:
    native_filesystem(const fs::path& root);

    bool exists(const std::string& name) const override;

protected:
    void map(
        const std::string& name, const uint8_t*& data, size_t& size
    ) override;

    void unmap(
        const std::string& name, const uint8_t* data, size_t size
    ) override;

private:
    fs::path root;
};

}

#endif

#include "filesystem.hh"
#include "error.hh"
#include <stdexcept>
#include <cstring>

namespace rb
{

file::file()
: data(nullptr), size(0), source(nullptr), read_head(0)
{
}

file::file(
    const std::string& name,
    const uint8_t* data,
    size_t size,
    filesystem* source
): name(name), data(data), size(size), source(source), read_head(0)
{
    source->ref(name);
}

file::file(const file& f)
: name(f.name), data(f.data), size(f.size), source(f.source), read_head(f.read_head)
{
    source->ref(name);
}

file::file(file&& f) noexcept
: name(std::move(f.name)), data(f.data), size(f.size), source(f.source), read_head(f.read_head)
{
    f.data = nullptr;
    f.size = 0;
    f.source = nullptr;
}

file::~file()
{
    if(source) source->unref(name);
}

const uint8_t* file::get_data() const
{
    return data;
}

size_t file::get_size() const
{
    return size;
}

const std::string& file::get_name() const
{
    return name;
}

std::string file::get_extension() const
{
    std::string ext = fs::path(name).extension().string();
    if(ext.size() > 0)
        return ext.substr(1);
    return ext;
}

filesystem* file::get_source_filesystem() const
{
    return source;
}

std::string file::get_string() const
{
    return std::string((const char*)data, size);
}

void file::seek(std::uint32_t offset)
{
    read_head = offset;
}

std::uint32_t file::tell() const
{
    return read_head;
}

bool file::finished() const
{
    return read_head >= size;
}

file::operator bool() const
{
    return read_head <= size;
}

file& file::operator=(const file& other)
{
    if(source) source->unref(name);
    name = other.name;
    data = other.data;
    size = other.size;
    source = other.source;
    source->ref(name);
    return *this;
}

file filesystem::get(const std::string& name)
{
    std::unique_lock lk(ref_lock);
    auto it = files.find(name);
    if(it == files.end())
    {
        it = files.insert({name, {nullptr, 0, 0}}).first;
        map(name, it->second.data, it->second.size);
    }

    return file(name, it->second.data, it->second.size, this);
}

file filesystem::operator[](const std::string& name)
{
    return get(name);
}

void filesystem::ref(const std::string& name)
{
    std::unique_lock lk(ref_lock);
    auto it = files.find(name);
    if(it == files.end())
        RB_PANIC("rb::filesystem: invalid ref! ", name);

    it->second.refcount++;
}

void filesystem::unref(const std::string& name)
{
    std::unique_lock lk(ref_lock);
    auto it = files.find(name);
    if(it == files.end())
        RB_PANIC("rb::filesystem: invalid ref! ", name);

    it->second.refcount--;
    if(it->second.refcount == 0)
    {
        unmap(name, it->second.data, it->second.size);
        files.erase(it);
    }
}

native_filesystem::native_filesystem(const fs::path& root)
: root(root)
{}

bool native_filesystem::exists(const std::string& name) const
{
    return fs::exists(root/name);
}

void native_filesystem::map(
    const std::string& name, const uint8_t*& data, size_t& size
){
    size = 0;
    data = nullptr;

    fs::path path = root/name;
    std::ifstream i(path, std::ifstream::binary);
    i.imbue(std::locale::classic());
    if(!i.is_open())
        RB_PANIC("Unable to read file ", path.string());

    i.seekg(0, i.end);
    size = i.tellg();
    i.seekg(0, i.beg);

    uint8_t* buf = new uint8_t[size];

    if(!i.read((char*)buf, size))
    {
        delete [] buf;
        RB_PANIC("Read failure! ", path.string());
    }

    data = buf;
}

void native_filesystem::unmap(const std::string&, const uint8_t* data, size_t)
{
    delete [] data;
}

}

#include "tjsCommHead.h"
#include "PlatformFile.h"
#include "TVPMsg.h"
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

static ttstr TVPLocalExtractFilePath(const ttstr& name)
{
    const tjs_char* p = name.c_str();
    tjs_int i = name.GetLen() - 1;
    for (; i >= 0; --i) if (p[i] == TJS_N(':') || p[i] == TJS_N('/') || p[i] == TJS_N('\\')) break;
    return ttstr(p, i + 1);
}

class tTVPLocalFileStream final : public tTJSBinaryStream {
    FILE* file_ = nullptr; ttstr name_;
public:
    tTVPLocalFileStream(const ttstr& original, const ttstr& local, tjs_uint32 flags) : name_(local) {
        const char* mode = (flags & TJS_BS_ACCESS_MASK) == TJS_BS_WRITE ? "wb+" :
                           (flags & TJS_BS_ACCESS_MASK) == TJS_BS_APPEND ? "ab+" :
                           (flags & TJS_BS_ACCESS_MASK) == TJS_BS_UPDATE ? "rb+" : "rb";
        if ((flags & TJS_BS_ACCESS_MASK) == TJS_BS_WRITE) TVPCreateFolders(TVPLocalExtractFilePath(local));
        file_ = std::fopen(local.AsStdString().c_str(), mode);
        if (!file_) TVPThrowExceptionMessage(TVPCannotOpenStorage, original);
    }
    ~tTVPLocalFileStream() { if (file_) std::fclose(file_); }
    tjs_uint64 Seek(tjs_int64 o, tjs_int w) override { return std::fseek(file_, (long)o, w) ? (tjs_uint64)-1 : (tjs_uint64)std::ftell(file_); }
    tjs_uint Read(void* b, tjs_uint n) override { return (tjs_uint)std::fread(b,1,n,file_); }
    tjs_uint Write(const void* b, tjs_uint n) override { return (tjs_uint)std::fwrite(b,1,n,file_); }
    bool Flush() override { return std::fflush(file_) == 0; }
    void SetEndOfStorage() override { std::fseek(file_,0,SEEK_END); }
    tjs_uint64 GetSize() override { long p=std::ftell(file_); std::fseek(file_,0,SEEK_END); long s=std::ftell(file_); std::fseek(file_,p,SEEK_SET); return s < 0 ? 0 : (tjs_uint64)s; }
};
tTJSBinaryStream* TVPCreateLocalFileStream(const ttstr& o,const ttstr& n,tjs_uint32 f){return new tTVPLocalFileStream(o,n,f);}
std::string TVPGetDefaultFileDir(){return std::filesystem::current_path().string();}
std::vector<std::string> TVPGetAppStoragePath(){return {TVPGetDefaultFileDir()};}
bool TVPCheckExistentLocalFile(const ttstr& n){struct stat s;return !n.IsEmpty()&&stat(n.AsStdString().c_str(),&s)==0&&S_ISREG(s.st_mode);}
bool TVPCheckExistentLocalFolder(const ttstr& n){struct stat s;return !n.IsEmpty()&&stat(n.AsStdString().c_str(),&s)==0&&S_ISDIR(s.st_mode);}
bool TVP_stat(const char* path, tTVP_stat& out) { struct stat s{}; if (stat(path, &s) != 0) return false; out.tvp_isdir = S_ISDIR(s.st_mode); out.tvp_size=s.st_size; out.tvp_atime=s.st_atime; out.tvp_mtime=s.st_mtime; out.tvp_ctime=s.st_ctime; return true; }
// TVP_utime is supplied by Core/environ/unix/unix_file.cpp.
std::string TVPSearchPath(const std::string& f,const std::string&){return TVPCheckExistentLocalFile(ttstr(f.c_str()))?f:"";}
bool TVPDeleteFile(const std::string& p){return std::remove(p.c_str())==0;}
bool TVPDeleteFolder(const std::string& p){return std::filesystem::remove(p);}
bool TVPRenameFile(const std::string&a,const std::string&b){return std::rename(a.c_str(),b.c_str())==0;}
bool TVPCopyFile(const std::string&a,const std::string&b){std::error_code e;std::filesystem::copy_file(a,b,std::filesystem::copy_options::overwrite_existing,e);return !e;}
bool TVPCreateFolders(const ttstr& p){std::error_code e;return p.IsEmpty()||std::filesystem::create_directories(p.AsStdString(),e)||!e;}
void TVPListDir(const std::string& p,std::function<void(const std::string&,int)> cb){
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(p, ec);
    if (ec) return;
    auto end = std::filesystem::directory_iterator();
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        cb(it->path().filename().string(), it->is_directory()?S_IFDIR:S_IFREG);
    }}
void TVPGetLocalFileListAt(const ttstr& p,const std::function<void(const ttstr&,tTVPLocalFileInfo*)>& cb){
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(p.AsStdString(), ec);
    if (ec) return;
    auto end = std::filesystem::directory_iterator();
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        tTVPLocalFileInfo i{};i.Mode=it->is_directory()?S_IFDIR:S_IFREG;
        ttstr n(it->path().filename().string().c_str());cb(n,&i);
    }}

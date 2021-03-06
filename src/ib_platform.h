#ifndef __IB_PLATFORM_H__
#define __IB_PLATFORM_H__

#include "ib_constants.h"
#include "ib_utils.h"

#ifdef IB_OS_WIN
#  include "ib_platform_win.h"
#endif
#ifdef IB_OS_LINUX
#  include "ib_platform_linux.h"
#endif

#include "ib_comp_value.h"

namespace ib {
  namespace platform {
    int  startup_system();
    int  init_system();
    void finalize_system();
    void get_runtime_platform(char *result);
    /* string conversion functions */
    std::unique_ptr<ib::oschar[]> utf82oschar(const char *src);
    void utf82oschar_b(ib::oschar *buf, const unsigned int bufsize, const char *src);
    std::unique_ptr<char[]> oschar2utf8(const ib::oschar *src);
    void oschar2utf8_b(char *buf, const unsigned int bufsize, const ib::oschar *src);
    std::unique_ptr<char[]> oschar2local(const ib::oschar *src);
    void oschar2local_b(char *buf, const unsigned int bufsize, const ib::oschar *src);
    std::unique_ptr<char[]> utf82local(const char *src);
    std::unique_ptr<char[]> local2utf8(const char *src);
    std::unique_ptr<ib::oschar[]> quote_string(ib::oschar *result, const ib::oschar *str);
    std::unique_ptr<ib::oschar[]> unquote_string(ib::oschar *result, const ib::oschar *str);

    /* application functions */
    void hide_window(Fl_Window *window);
    void activate_window(Fl_Window *window);
    void raise_window(Fl_Window *window);
    void set_window_alpha(Fl_Window *window, int alpha);
    int  shell_execute(const std::string &path, const std::vector<std::unique_ptr<std::string>> &params, const std::string &cwd, const std::string &terminal, bool sudo, ib::Error& error);
    int  shell_execute(const std::string &path, const std::vector<std::string*> &params, const std::string &cwd, const std::string &terminal, bool sudo, ib::Error& error);
    int command_output(std::string &sstdout, std::string &sstderr, const char *command, ib::Error &error);
    int show_context_menu(ib::oschar *path);
    void on_command_init(ib::Command *command);
    ib::oschar* default_config_path(ib::oschar *result);
    ib::oschar* resolve_icon(ib::oschar *result, ib::oschar *file, int size);

    /* path functions */
    Fl_Image* get_associated_icon_image(const ib::oschar *path, const int size);
    ib::oschar* join_path(ib::oschar *result, const ib::oschar *parent, const ib::oschar *child);
    ib::oschar* normalize_path(ib::oschar *result, const ib::oschar *path);
    ib::oschar* normalize_join_path(ib::oschar *result, const ib::oschar *parent, const ib::oschar *child);
    ib::oschar* dirname(ib::oschar *result, const ib::oschar *path);
    ib::oschar* basename(ib::oschar *result, const ib::oschar *path);
    ib::oschar* to_absolute_path(ib::oschar *result, const ib::oschar *dir, const ib::oschar *path);
    bool is_directory(const ib::oschar *path);
    bool is_path(const ib::oschar *path);
    bool is_relative_path(const ib::oschar *path);
    bool directory_exists(const ib::oschar *path);
    bool file_exists(const ib::oschar *path);
    bool path_exists(const ib::oschar *path);
    int walk_dir(std::vector<std::unique_ptr<ib::oschar[]>> &result, const ib::oschar *dir, ib::Error &error, bool recursive = false);
    ib::oschar* get_self_path(ib::oschar *result);
    ib::oschar* get_current_workdir(ib::oschar *result);
    int set_current_workdir(const ib::oschar *dir, ib::Error &error);
    bool which(ib::oschar *result, const ib::oschar *name);
    int list_drives(std::vector<std::unique_ptr<ib::oschar[]>> &result, ib::Error &error);
    ib::oschar* icon_cache_key(ib::oschar *result, const ib::oschar *path);

    /* filesystem functions */
    int remove_file(const ib::oschar *path, ib::Error &error);
    int copy_file(const ib::oschar *source, const ib::oschar *dest, ib::Error &error);
    int file_size(size_t &size, const ib::oschar *path, ib::Error &error);
    ib::oschar* file_type(ib::oschar *result, const ib::oschar *path);

    /* thread functions */
    void create_thread(ib::thread *t, ib::threadfunc f, void* p);
    void on_thread_start();
    void join_thread(ib::thread *t);
    void exit_thread(int exit_code);
    void create_mutex(ib::mutex *m);
    void destroy_mutex(ib::mutex *m);
    void lock_mutex(ib::mutex *m);
    void unlock_mutex(ib::mutex *m);
    void create_cmutex(ib::cmutex *m);
    void destroy_cmutex(ib::cmutex *m);
    void lock_cmutex(ib::cmutex *m);
    void unlock_cmutex(ib::cmutex *m);
    void create_condition(ib::condition *c);
    void destroy_condition(ib::condition *c);
    int  wait_condition(ib::condition *c, ib::cmutex *m, int ms);
    void notify_condition(ib::condition *c);

    /* process functions */
    int wait_pid(const int pid);
    int get_pid();

    /* dynamic loading functions */
    int load_library(ib::module &dl, const ib::oschar *name, ib::Error &error);
    void* get_dynamic_symbol(ib::module dl, const char *name);
    void close_library(ib::module dl);

    /* socket functions */
    void close_socket(FL_SOCKET s);

    /* system functions */
    int get_num_of_cpu();
    int convert_keysym(int key);

    class ScopedLock : private NonCopyable<ScopedLock> { // {{{
      public:
        explicit ScopedLock(ib::mutex *mutex) : mutex_(mutex) { ib::platform::lock_mutex(mutex_); }
        ~ScopedLock(){ ib::platform::unlock_mutex(mutex_); }
      protected:
        ib::mutex *mutex_;
    }; // }}}

  }
}

#endif

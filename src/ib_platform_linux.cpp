#include "ib_platform.h"
#include "ib_resource.h"
#include "ib_utils.h"
#include "ib_ui.h"
#include "ib_controller.h"
#include "ib_config.h"
#include "ib_regex.h"
#include "ib_comp_value.h"

static char ib_g_lang[32];
static int  ib_g_hotkey;

// utilities {{{
static bool string_startswith(const char *str, const char *pre) {
  return strncmp(pre, str, strlen(pre)) == 0;
}

static bool string_endswith(const char *str, const char *pre) {
  const char *dot = strrchr(str, '.');
  return (dot && !strcmp(dot, pre));
}

static void parse_desktop_entry_value(std::string &result, std::string &value) {
  ib::Regex re("(\\\\s|\\\\n|\\\\t|\\\\r|\\\\\\\\)", ib::Regex::NONE);
  re.init();
  re.gsub(result, value, [](const ib::Regex &reg, std::string *res, void *userdata) {
      std::string escape;
      reg._1(escape);
      if(escape == "\\s") *res += " ";
      else if(escape == "\\n") *res += "\n";
      else if(escape == "\\t") *res += "\t";
      else if(escape == "\\r") *res += "\r";
      else if(escape == "\\\\") *res += "\\";
  }, 0);
}

static void normalize_desktop_entry_value(std::string &result, std::string &value) {
  ib::Regex re("(\\n|\\r|\\t)", ib::Regex::NONE);
  re.init();
  re.gsub(result, value, [](const ib::Regex &reg, std::string *res, void *userdata) {
      std::string escape;
      reg._1(escape);
      if(escape == "\t") *res += "    ";
  }, 0);
}

static int read_fd_all(int fd, std::string &out) {
  char buf[1024];
  do{
    ssize_t ret = read(fd, buf, 1024);
    if(ret < 0) {
      return -1;
    } else {
      buf[ret] = '\0';
      out += buf;
      if(ret == 0) break;
    }
  }while(1);
  return 0;
}

static int parse_cmdline(std::vector<ib::unique_char_ptr> &result, const char *cmdline) {
  ib::Regex space_r("\\s+", ib::Regex::I);
  ib::Regex string_r("\"((((?<=\\\\)\")|[^\"])*)((?<!\\\\)\")", ib::Regex::I);
  ib::Regex value_r("([^\\(\\[\\)\\]\\s\"]+)", ib::Regex::I);
  space_r.init();
  string_r.init();
  value_r.init();
  std::size_t pos = 0;
  std::size_t len = strlen(cmdline);
  while(pos < len) {
    std::string cap;
    if(space_r.match(cmdline, pos) == 0) {
      pos = space_r.getEndpos(0) + 1;
    } else if(string_r.match(cmdline, pos) == 0) {
      string_r._1(cap);
      char *buf = new char[cap.length()+1];
      strcpy(buf, cap.c_str());
      result.push_back(ib::unique_char_ptr(buf));
      pos = string_r.getEndpos(0) + 1;
    } else if(value_r.match(cmdline, pos) == 0) {
      value_r._1(cap);
      char *buf = new char[cap.length()+1];
      strcpy(buf, cap.c_str());
      result.push_back(ib::unique_char_ptr(buf));
      pos = value_r.getEndpos(0) + 1;
    } else {
      return -1;
    }
  }
  return 0;
}

static void set_errno(ib::Error &error) {
  char buf[1024];
  error.setMessage(strerror_r(errno, buf, 1024));
  error.setCode(errno);
}

// }}}

//////////////////////////////////////////////////
// X11 functions {{{
//////////////////////////////////////////////////

static int xerror_handler(Display* d, XErrorEvent* e) {
  char buf1[128], buf2[128];
  sprintf(buf1, "XRequest.%d", e->request_code);
  XGetErrorDatabaseText(d,"",buf1,buf1,buf2,128);
  XGetErrorText(d, e->error_code, buf1, 128);
  //Fl::warning("%s: %s: %s 0x%lx", program_name, buf2, buf1, e->resourceid);
  printf("xerror: %s(%d): %s 0x%lx\n", buf2, e->error_code, buf1, e->resourceid);
  return 0;
}

static int xevent_handler(int e){
  if (!e) {
    //Window window = fl_xevent->xany.window;
    switch(fl_xevent->type){
      case KeyPress: // hotkey
        ib::Controller::inst().showApplication();
        return 1;
    }
  } 
  return 0;
}

static int xregister_hotkey() {
  Window root = RootWindow(fl_display, fl_screen);
  const int* hot_key = ib::Config::inst().getHotKey();
  for(int i=0 ;hot_key[i] != 0; ++i){
    ib_g_hotkey += hot_key[i];
  }
  const int keycode = XKeysymToKeycode(fl_display, ib_g_hotkey & 0xFFFF);
  if(!keycode) {
    return -1;
  }
  XGrabKey(fl_display, keycode, ib_g_hotkey >>16,     root, 1, GrabModeAsync, GrabModeAsync);
  XGrabKey(fl_display, keycode, (ib_g_hotkey>>16)|2,  root, 1, GrabModeAsync, GrabModeAsync); // CapsLock
  XGrabKey(fl_display, keycode, (ib_g_hotkey>>16)|16, root, 1, GrabModeAsync, GrabModeAsync); // NumLock
  XGrabKey(fl_display, keycode, (ib_g_hotkey>>16)|18, root, 1, GrabModeAsync, GrabModeAsync); // both
  return 0;
}
//////////////////////////////////////////////////
// }}}
//////////////////////////////////////////////////

int ib::platform::startup_system() { // {{{
   XSetErrorHandler(xerror_handler);
  // TODO get user LANG from ENV.
  strncpy(ib_g_lang, "UTF-8", 32 - 1);
  return 0;
} // }}}

int ib::platform::init_system() { // {{{
  ib::ListWindow::inst()->set_menu_window();
  XAllowEvents(fl_display, AsyncKeyboard, CurrentTime);
  XkbSetDetectableAutoRepeat(fl_display, true, NULL);
  if(xregister_hotkey() < 0 ) {
    fl_alert("%s", "Failed to register hotkeys.");
    ib::utils::exit_application(1);
  }

  Fl::add_handler(xevent_handler);
  XFlush(fl_display);
  return 0;
} // }}}

void ib::platform::finalize_system(){ // {{{ 
  Window root = XRootWindow(fl_display, fl_screen);
  const int keycode = XKeysymToKeycode(fl_display, ib_g_hotkey & 0xFFFF);
  XUngrabKey(fl_display, keycode, ib_g_hotkey >>16,     root);
  XUngrabKey(fl_display, keycode, (ib_g_hotkey>>16)|2,  root); // CapsLock
  XUngrabKey(fl_display, keycode, (ib_g_hotkey>>16)|16, root); // NumLock
  XUngrabKey(fl_display, keycode, (ib_g_hotkey>>16)|18, root); // both
} // }}}

void ib::platform::get_runtime_platform(char *ret){ // {{{ 
} // }}}

ib::oschar* ib::platform::utf82oschar(const char *src) { // {{{
  char *buff = new char[strlen(src)+1];
  strcpy(buff, src);
  return buff;
} // }}}

void ib::platform::utf82oschar_b(ib::oschar *buf, const unsigned int bufsize, const char *src) { // {{{
  strncpy(buf, src, bufsize-1);
} // }}}

char* ib::platform::oschar2utf8(const ib::oschar *src) { // {{{
  char *buff = new char[strlen(src)+1];
  strcpy(buff, src);
  return buff;
} // }}}

void ib::platform::oschar2utf8_b(char *buf, const unsigned int bufsize, const ib::oschar *src) { // {{{
  strncpy(buf, src, bufsize-1);
} // }}}

char* ib::platform::oschar2local(const ib::oschar *src) { // {{{
  char *buff = new char[strlen(src)+1];
  strcpy(buff, src);
  return buff;
} // }}}

void ib::platform::oschar2local_b(char *buf, const unsigned int bufsize, const ib::oschar *src) { // {{{
  strncpy(buf, src, bufsize-1);
} // }}}

char* ib::platform::utf82local(const char *src) { // {{{
  char *buff = new char[strlen(src)+1];
  strcpy(buff, src);
  return buff;
} // }}}

char* ib::platform::local2utf8(const char *src) { // {{{
  char *buff = new char[strlen(src)+1];
  strcpy(buff, src);
  return buff;
} // }}}

ib::oschar* ib::platform::quote_string(ib::oschar *result, const ib::oschar *str) { // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  bool need_quote = false;
  for(const ib::oschar *tmp = str; *tmp != '\0'; tmp++) {
    if(isspace(*tmp)) { need_quote = true; break; }
  }
  if((strlen(str) != 0 && str[0] == '"') || !need_quote) {
    strncpy(result, str, IB_MAX_PATH-1);
    return result;
  }

  ib::oschar *cur = result + 1;
  result[0] = '"';

  for(std::size_t i =0 ;*str != '\0' && i != IB_MAX_PATH-2; cur++, str++, i++){
    if(*str == '"'){
      *cur = '\\';
      cur++;
    }
    *cur = *str;
  }
  *cur = '"';

  return result;
} // }}}

ib::oschar* ib::platform::unquote_string(ib::oschar *result, const ib::oschar *str) { // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  std::size_t len = strlen(str);
  if(len == 0) { result[0] = '\0'; return result; }
  if(len == 1) { strcpy(result, str); return result; }
  if(*str == '"') str++;
  strncpy(result, str, IB_MAX_PATH-1);
  len = strlen(result);
  if(result[len-1] == '"') {
    result[len-1] = '\0';
  }
  return result;
} // }}}

void ib::platform::hide_window(Fl_Window *window){ // {{{
  XIconifyWindow(fl_display, fl_xid(window), fl_screen);
  XFlush(fl_display);
} // }}}

void ib::platform::show_window(Fl_Window *window){ // {{{
  Atom wm_states[3];
  Atom wm_state;
  wm_state = XInternAtom(fl_display, "_NET_WM_STATE", False);
  wm_states[1] = XInternAtom(fl_display, "_NET_WM_STATE_SKIP_TASKBAR", False);
  XChangeProperty(fl_display, fl_xid(window), wm_state, XA_ATOM, 32, PropModeReplace,(unsigned char *) wm_states, 3);
  XClientMessageEvent ev = {0};
  ev.type = ClientMessage;
  ev.window = fl_xid(window);
  ev.message_type = XInternAtom(fl_display, "_NET_ACTIVE_WINDOW", True);
  ev.format = 32;
  ev.data.l[0] = 1;
  ev.data.l[1] = CurrentTime;
  ev.data.l[2] = ev.data.l[3] = ev.data.l[4] = 0;
  XSendEvent (fl_display, RootWindow(fl_display, fl_screen), 0,
    SubstructureRedirectMask |SubstructureNotifyMask, (XEvent*)&ev);

  XFlush(fl_display);
} // }}}

void ib::platform::set_window_alpha(Fl_Window *window, int alpha){ // {{{
  int visible = window->visible();
  if(!visible) window->show();
  double  a = alpha/255.0;
  uint32_t cardinal_alpha = (uint32_t) (a * (uint32_t)-1) ;
  XChangeProperty(fl_display, fl_xid(window), 
                 XInternAtom(fl_display, "_NET_WM_WINDOW_OPACITY", 0),
                 XA_CARDINAL, 32, PropModeReplace, (uint8_t*)&cardinal_alpha, 1) ;
  if(!visible) window->hide();
} // }}}

static int ib_platform_shell_execute(const std::string &path, const std::string &strparams, const std::string &cwd, ib::Error &error) { // {{{
  std::string cmd;
  int ret;

  ib::oschar wd[IB_MAX_PATH];
  ib::platform::get_current_workdir(wd);
  if(ib::platform::set_current_workdir(cwd.c_str(), error) != 0) {
    return -1;
  };

  ib::Regex proto_reg("^(\\w+)://.*", ib::Regex::I);
  proto_reg.init();
  ret = 0;
  if(proto_reg.match(path) == 0){
    cmd += "xdg-open '";
    cmd += path;
    cmd += "' ";
    cmd += strparams;
  } else {
    if(-1 == access(path.c_str(), R_OK)) {
      set_errno(error);
      ret = -1;
      goto finally;
    }
    if(!strparams.empty() || access(path.c_str(), X_OK) == 0) {
      std::string cmd;
      cmd += path;
      cmd += " ";
      cmd += strparams;
    }
  }

  ret = system((cmd + " &").c_str());
  if(ret < 0) {
    std::string message;
    message += "Failed to start ";
    message += path;
    error.setCode(1);
    error.setMessage(message.c_str());
    goto finally;
  }

finally:
  
  ib::platform::set_current_workdir(wd, error);
  return ret;
} // }}}

int ib::platform::shell_execute(const std::string &path, const std::vector<ib::unique_string_ptr> &params, const std::string &cwd, ib::Error &error) { // {{{
  std::string strparams;
  for(auto it = params.begin(), last = params.end(); it != last; ++it){
    ib::oschar qparam[IB_MAX_PATH];
    strparams += " ";
    ib::platform::quote_string(qparam, (*it).get()->c_str());
    strparams += qparam;
  }
  return ib_platform_shell_execute(path, strparams, cwd, error);
} /* }}} */

int ib::platform::shell_execute(const std::string &path, const std::vector<std::string*> &params, const std::string &cwd, ib::Error &error) { // {{{
  std::string strparams;
  for(auto it = params.begin(), last = params.end(); it != last; ++it){
    ib::oschar qparam[IB_MAX_PATH];
    strparams += " ";
    ib::platform::quote_string(qparam, (*it)->c_str());
    strparams += qparam;
  }
  return ib_platform_shell_execute(path, strparams, cwd, error);
} /* }}} */

int ib::platform::command_output(std::string &sstdout, std::string &sstderr, const char *cmd, ib::Error &error) { // {{{
  int outfd[2];
  int infd[2];
  int einfd[2];
  int oldstdin, oldstdout, oldstderr;

  std::vector<ib::unique_char_ptr> argv;
  if(parse_cmdline(argv, cmd) != 0) {
    error.setMessage("Invalid command line.");
    error.setCode(1);
    return 1;
  }
  std::unique_ptr<char*[]> cargv(new char*[argv.size()+1]);
  for(std::size_t i = 0; i < argv.size(); i++) {
    cargv.get()[i] = argv.at(i).get();
  }
  cargv[argv.size()] = NULL;

  if(pipe(outfd) != 0 || pipe(infd) != 0 || pipe(einfd) != 0) {
    error.setMessage("Failed to create pipes.");
    error.setCode(1);
    return 1;
  }
  oldstdin = dup(0);
  oldstdout = dup(1);
  oldstderr = dup(2);
  close(0);
  close(1);
  close(2);
  dup2(outfd[0], 0);
  dup2(infd[1],1);
  dup2(einfd[1],2);
  pid_t pid = fork();
  if(pid < 0) {
    set_errno(error);
    return 1;
  } else if(!pid){
    close(outfd[0]);
    close(outfd[1]);
    close(infd[0]);
    close(infd[1]);
    close(einfd[0]);
    close(einfd[1]);
    execv(cargv.get()[0],cargv.get());
  } else {
    close(0);
    close(1);
    close(2);
    dup2(oldstdin, 0);
    dup2(oldstdout, 1);
    dup2(oldstderr, 2);
    
    close(outfd[0]);
    close(infd[1]);
    close(einfd[1]);
    
    int status;
    pid_t ret = waitpid(pid, &status, 0);
    if (ret < 0) {
      set_errno(error);
      return 1;
    }
    read_fd_all(infd[0], sstdout); // ignore errors
    read_fd_all(einfd[0], sstderr); // ignore errors
    return WEXITSTATUS(status);
  }
  return 1;
} // }}}

void ib::platform::list_all_windows(std::vector<ib::whandle> &result){ // {{{
} // }}}

int ib::platform::show_context_menu(ib::oschar *path){ // {{{
  return 0;
} // }}}

void ib::platform::on_command_init(ib::Command *cmd) { // {{{
  const char *path = cmd->getCommandPath().c_str();
  if(string_endswith(path, ".desktop")){
    std::ifstream ifs(path);
    std::string   line;
    if (ifs.fail()) {
      return; // ignore errors;
    }
    ib::Regex kv_reg("^([^#][^=\\s]+)\\s*=\\s*(.*)$", ib::Regex::I);
    kv_reg.init();
    ib::Regex section_reg("^\\s*\\[([^\\]]+)]", ib::Regex::I);
    section_reg.init();

    ib::string_map props;
    std::string current_section;
    while(std::getline(ifs, line)) {
      if(section_reg.match(line.c_str()) == 0) {
        section_reg._1(current_section);
      } if(kv_reg.match(line.c_str()) == 0 && current_section == "Desktop Entry") {
        std::string   key;
        std::string   raw_value;
        std::string   value;
        kv_reg._1(key);
        kv_reg._2(raw_value);
        parse_desktop_entry_value(value, raw_value);
        props.insert(ib::string_pair(key, value));
      }
    }
    // TODO support localizations.

    auto prop_type = props.find("Type");
    if(prop_type == props.end()) return; // Type is a required value. ignore errors;

    auto prop_hidden = props.find("Hidden");
    if(prop_hidden != props.end() &&
       (*prop_hidden).second == "true" ) {
      cmd->setName("/");
      return; // ignore this command. '/' is treated as a path, thus this command will never be shown in the completion lists.
    }

    auto prop_name = props.find("Name");
    if(prop_name == props.end()) return; // Name is a required value. ignore errors;
    std::string name;
    ib::utils::to_command_name(name, (*prop_name).second);
    cmd->setName(name);

    auto prop_comment = props.find("Comment");
    if(prop_comment != props.end()) {
      std::string normalized;
      normalize_desktop_entry_value(normalized, (*prop_comment).second);
      cmd->setDescription(normalized);
    }

    auto prop_icon = props.find("Icon");
    if(prop_icon != props.end()) {
      //TODO resolve icon file path?
      cmd->setIconFile((*prop_icon).second);
    }

    if((*prop_type).second == "Application") {
      auto prop_path = props.find("Path");
      if(prop_path != props.end()) {
        cmd->setWorkdir((*prop_path).second);
      }
    } else if((*prop_type).second == "Directory") {
    } else if((*prop_type).second == "Link") {
    }
  }
} // }}}

ib::oschar* ib::platform::default_config_path(ib::oschar *result) { // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  snprintf(result, IB_MAX_PATH-1, "%s/%s", getenv("HOME"), ".iceberg");
  return result;
} // }}}

//////////////////////////////////////////////////
// path functions {{{
//////////////////////////////////////////////////
Fl_RGB_Image* ib::platform::get_associated_icon_image(const ib::oschar *path, const int size){ // {{{
  return 0;
} /* }}} */

ib::oschar* ib::platform::join_path(ib::oschar *result, const ib::oschar *parent, const ib::oschar *child) { // {{{
  if(result == 0){
    result = new ib::oschar[IB_MAX_PATH];
  }
  strncpy(result, parent, IB_MAX_PATH-1);
  std::size_t length = strlen(result);
  const ib::oschar *sep;
  if(result[length-1] != '/') {
    sep = "/";
  }else{
    sep = "";
  }
  strncat(result, sep, IB_MAX_PATH-length);
  strncat(result, child, IB_MAX_PATH-length-1);
  return result;
} // }}}

ib::oschar* ib::platform::normalize_path(ib::oschar *result, const ib::oschar *path){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  result[0] = '\0';
  ib::oschar tmp[IB_MAX_PATH];
  bool is_dot_path = string_startswith(path, "./") || strcmp(path, ".") == 0;
  bool is_dot_dot_path = string_startswith(path, "../") || strcmp(path, "..") == 0;
  strncpy(tmp, path, IB_MAX_PATH-1);
  if(is_dot_path){
    tmp[0] = '_';
  }else if(is_dot_dot_path){
    tmp[0] = '_'; tmp[1] = '_';
  }
  ib::Regex sep("/", ib::Regex::I);
  sep.init();
  std::vector<std::string*> parts;
  std::vector<std::string*>  st;
  sep.split(parts, path);
  for(auto part : parts) {
    if(*part == ".") continue;
    if(*part == ".." && st.size() > 0){
      st.pop_back();
    } else {
      st.push_back(part);
    }
  }
  for(auto part : st) {
    strcat(result, part->c_str());
    strcat(result, "/");
  }
  if(path[strlen(path)-1] != '/') {
    result[strlen(result)-1] = '\0';
  }
  if(is_dot_path){
    result[0] = '.';
  }else if(is_dot_dot_path){
    result[0] = '.'; result[1] = '.';
  }
  ib::utils::delete_pointer_vectors(parts);
  return result;
} // }}}

ib::oschar* ib::platform::normalize_join_path(ib::oschar *result, const ib::oschar *parent, const ib::oschar *child){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  ib::oschar tmp[IB_MAX_PATH];
  ib::platform::join_path(tmp, parent, child);
  ib::platform::normalize_path(result, tmp);
  return result;
} // }}}

ib::oschar* ib::platform::dirname(ib::oschar *result, const ib::oschar *path){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  strcpy(result, path);
  const std::size_t len = strlen(path);
  if(len == 0) return result;
  std::size_t i = len -1;
  for(; i > 0; --i){
    if(result[i] == '/'){
      break;
    }
  }
  result[i] = '\0';
  return result;
} // }}}

ib::oschar* ib::platform::basename(ib::oschar *result, const ib::oschar *path){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  ib::oschar tmp[IB_MAX_PATH];
  strncpy(tmp, path, IB_MAX_PATH-1);
  const char *basename = fl_filename_name(tmp);
  strncpy(result, basename, IB_MAX_PATH-1);
  return result;
} // }}}

ib::oschar* ib::platform::to_absolute_path(ib::oschar *result, const ib::oschar *dir, const ib::oschar *path) { // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  if(ib::platform::is_relative_path(path)){
    ib::platform::normalize_join_path(result, dir, path);
  } else {
    strncpy(result, path, IB_MAX_PATH-1);
  }
  return result;
} // }}}

bool ib::platform::is_directory(const ib::oschar *path) { // {{{
  const std::size_t length = strlen(path);
  return (path[length-1] == '/' || fl_filename_isdir(path) != 0);
} // }}}

bool ib::platform::is_path(const ib::oschar *str){ // {{{
  const std::size_t length = strlen(str);
  return (length > 0 && str[0] == '/') || 
         (length > 1 && str[0] == '.' && str[1] == '/') ||
         (length > 2 && str[0] == '.' && str[1] == '.' && str[2] == '/');
} // }}}

bool ib::platform::is_relative_path(const ib::oschar *path) { // {{{
  const std::size_t length = strlen(path);
  bool is_dot_path = length > 1 && path[0] == '.' && path[1] == '/';
  if(!is_dot_path) is_dot_path = length == 1 && path[0] == '.';
  bool is_dot_dot_path = length > 2 && path[0] == '.' && path[1] == '.' && path[2] == '/';
  if(!is_dot_dot_path) is_dot_dot_path = length == 2 && path[0] == '.' && path[1] == '.';
  return is_dot_path || is_dot_dot_path;
} // }}}

bool ib::platform::directory_exists(const ib::oschar *path) { // {{{
  return fl_filename_isdir(path) ? true : false;
} // }}}

bool ib::platform::file_exists(const ib::oschar *path) { // {{{
  return ib::platform::path_exists(path) && !ib::platform::directory_exists(path);
} // }}}

bool ib::platform::path_exists(const ib::oschar *path) { // {{{
  return access(path, F_OK) == 0;
} // }}}

int ib::platform::walk_dir(std::vector<ib::unique_oschar_ptr> &result, const ib::oschar *dir, ib::Error &error, bool recursive) { // {{{
    DIR *d = opendir(dir);
    if (!d) {
      set_errno(error);
      return 1;
    }
    while (1) {
        struct dirent * entry = readdir(d);
        if (!entry) break;
        const char *d_name = entry->d_name;
        if (entry->d_type & DT_DIR) {
            if (strcmp (d_name, "..") != 0 &&
                strcmp (d_name, ".") != 0) {
                ib::oschar *path = new ib::oschar[IB_MAX_PATH];
                if(recursive){
                  snprintf(path, IB_MAX_PATH-1, "%s/%s", dir, d_name);
                } else {
                  snprintf(path, IB_MAX_PATH-1, "%s", d_name);
                }
                result.push_back(ib::unique_oschar_ptr(path));
                if(recursive) {
                  ib::platform::walk_dir(result, path, error, recursive);
                }
            }
        } else {
          ib::oschar *path = new ib::oschar[IB_MAX_PATH];
          if(recursive){
            snprintf(path, IB_MAX_PATH-1, "%s/%s", dir, d_name);
          } else {
            snprintf(path, IB_MAX_PATH-1, "%s", d_name);
          }
          result.push_back(ib::unique_oschar_ptr(path));
        }
    }
    if(closedir(d)){
      // ignore errors...
    }
    return 0;
} // }}}

ib::oschar* ib::platform::get_self_path(ib::oschar *result){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  std::size_t ret = readlink("/proc/self/exe", result, IB_MAX_PATH-1);
  if(ret < 0) {
    fl_alert("Failed to get self path.");
  }
  return result;
} // }}}

ib::oschar* ib::platform::get_current_workdir(ib::oschar *result){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  if(NULL == getcwd(result, IB_MAX_PATH-1)) {
    // ignore errors;
  }
  return result;
} // }}}

int ib::platform::set_current_workdir(const ib::oschar *dir, ib::Error &error){ // {{{
  if(chdir(dir)!= 0) {
    set_errno(error);
    return -1;
  }
  return 0;
} // }}}

bool ib::platform::which(ib::oschar *result, const ib::oschar *name) { // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  char *filename = result;
  const char* program = name;
  std::size_t filesize = IB_MAX_PATH;

  // code from fl_open_uri.cxx
  const char *path;
  char       *ptr,
             *end;
  if ((path = getenv("PATH")) == NULL) path = "/bin:/usr/bin";
  for (ptr = filename, end = filename + filesize - 1; *path; path ++) {
    if (*path == ':') {
      if (ptr > filename && ptr[-1] != '/' && ptr < end) *ptr++ = '/';
      strncpy(ptr, program, end - ptr);
      if (!access(filename, X_OK)) return true;
      ptr = filename;
    } else if (ptr < end) *ptr++ = *path;
  }
  if (ptr > filename) {
    if (ptr[-1] != '/' && ptr < end) *ptr++ = '/';
    strncpy(ptr, program, end - ptr);
    if (!access(filename, X_OK)) return true;
  }
  return false;
} // }}}

ib::oschar* ib::platform::icon_cache_key(ib::oschar *result, const ib::oschar *path) { // {{{
  return NULL;
} // }}}

//////////////////////////////////////////////////
// path functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// filesystem functions {{{
//////////////////////////////////////////////////
int ib::platform::remove_file(const ib::oschar *path, ib::Error &error){ // {{{
  if(remove(path) < 0) {
    set_errno(error);
    return -1;
  }
  return 0;
} // }}}

int ib::platform::copy_file(const ib::oschar *source, const ib::oschar *dest, ib::Error &error){ // {{{
  int fdin = -1, fdout = -1, ret = 0;
  off_t ncpy = 0;
  struct stat fileinfo = {0};

  if((fdin = open(source, O_RDONLY)) < 0){
    ret = -1;
    goto finally;
  }
  if(fstat(fdin, &fileinfo) < 0) {
    ret = -1;
    goto finally;
  }

  if((fdout = open(dest,  O_WRONLY|O_TRUNC| O_CREAT, fileinfo.st_mode & 07777)) < 0){
    ret = -1;
    goto finally;
  }
  if(sendfile(fdout, fdin, &ncpy, fileinfo.st_size) < 0) {
    ret = -1;
    goto finally;
  }

finally:
  if(fdin >= 0) close(fdin);
  if(fdout >= 0) close(fdout);
  if(ret < 0){
    set_errno(error);
  }
  return ret;
} // }}}

int ib::platform::file_size(size_t &size, const ib::oschar *path, ib::Error &error){ // {{{
  struct stat st;
  if(stat(path, &st) < 0) {
    set_errno(error);
    return -1;
  }
  size = (size_t)st.st_size;
  return 0;
} // }}}

ib::oschar* ib::platform::file_type(ib::oschar *result, const ib::oschar *path){ // {{{
  if(result == 0){ result = new ib::oschar[IB_MAX_PATH]; }
  const char *dot = strrchr(path, '.');
  if(!dot || dot == path) return result;
  strcpy(result, dot);
  return result;
} // }}}
//////////////////////////////////////////////////
// filesystem functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// thread functions {{{
//////////////////////////////////////////////////
void ib::platform::create_thread(ib::thread *t, ib::threadfunc f, void* p) { // {{{
  pthread_create(t, NULL, f, p);
}
/* }}} */

void ib::platform::on_thread_start(){ /* {{{ */
} /* }}} */

void ib::platform::join_thread(ib::thread *t){ /* {{{ */
  pthread_join(*t, NULL);
} /* }}} */

void ib::platform::exit_thread(int exit_code) { // {{{
  // nothing to do
} // }}}

void ib::platform::create_mutex(ib::mutex *m) { /* {{{ */
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr); 
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
  int ret = pthread_mutex_init(m, &attr);
  if(ret == 0) {
    m = 0;
  }
} /* }}} */

void ib::platform::destroy_mutex(ib::mutex *m) { /* {{{ */
  // nothing to do
} /* }}} */

void ib::platform::lock_mutex(ib::mutex *m) { /* {{{ */
  pthread_mutex_lock(m);
} /* }}} */

void ib::platform::unlock_mutex(ib::mutex *m) { /* {{{ */
  pthread_mutex_unlock(m);
} /* }}} */

void ib::platform::create_cmutex(ib::cmutex *m) { /* {{{ */
  *m = PTHREAD_MUTEX_INITIALIZER;
} /* }}} */

void ib::platform::destroy_cmutex(ib::cmutex *m) { /* {{{ */
  // nothing to do
} /* }}} */

void ib::platform::lock_cmutex(ib::cmutex *m) { /* {{{ */
  pthread_mutex_lock(m);
} /* }}} */

void ib::platform::unlock_cmutex(ib::cmutex *m) { /* {{{ */
  pthread_mutex_unlock(m);
} /* }}} */

void ib::platform::create_condition(ib::condition *c) { /* {{{ */
  pthread_cond_init(c, NULL);
} /* }}} */

void ib::platform::destroy_condition(ib::condition *c) { /* {{{ */
  pthread_cond_destroy(c);
} /* }}} */

int ib::platform::wait_condition(ib::condition *c, ib::cmutex *m, int ms) { /* {{{ */
  if(ms == 0) return pthread_cond_wait(c, m);
  struct timespec tv;
  tv.tv_sec  = time(NULL) + ms/1000;
  tv.tv_nsec = ms%1000 * 1000000;
  return pthread_cond_timedwait(c, m, &tv);
} /* }}} */

void ib::platform::notify_condition(ib::condition *c) { /* {{{ */
  pthread_cond_signal(c);
} /* }}} */

//////////////////////////////////////////////////
// thread functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// process functions {{{
//////////////////////////////////////////////////
int ib::platform::wait_pid(const int pid) { // {{{
  for(int limit = 0; limit < 10; limit++){
    if(kill(pid, 0) < 0 && errno == ESRCH) {
      return 0;
    }
    struct timespec ts = {0};
    ts.tv_nsec = 500 * 1000000;
    nanosleep(&ts, 0);
  }
  return -1;
} // }}}

int ib::platform::get_pid() { // {{{
  return (int)getpid();
} // }}}

//////////////////////////////////////////////////
// process functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// dynamic loading functions {{{
//////////////////////////////////////////////////
int ib::platform::load_library(ib::module &dl, const ib::oschar *name, ib::Error &error) { // {{{
  dl = dlopen(name, RTLD_LAZY);
  if(dl == 0) {
    error.setMessage(dlerror());
    error.setCode(1);
    return 1;
  }
  return 0;
} // }}}

void* ib::platform::get_dynamic_symbol(ib::module dl, const char *name){ //{{{
  return dlsym (dl, name);
} //}}}

void ib::platform::close_library(ib::module dl) { //{{{
  dlclose(dl);
} //}}}

//////////////////////////////////////////////////
// dynamic loading functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// socket functions {{{
//////////////////////////////////////////////////
void ib::platform::close_socket(FL_SOCKET s){ // {{{
  close(s);
} // }}}
//////////////////////////////////////////////////
// socket functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// system functions {{{
//////////////////////////////////////////////////
int ib::platform::get_num_of_cpu(){ // {{{
  return (int)sysconf(_SC_NPROCESSORS_ONLN);
} // }}}
//////////////////////////////////////////////////
// system functions }}}
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// platform specific functions {{{
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// platform specific functions }}}
//////////////////////////////////////////////////

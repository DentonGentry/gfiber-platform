#include "dir-monitor.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

using namespace std;


int do_output_modify_events = 0;


static int is_dir(const char *dir_path)
{
  // Sanity checks.
  struct stat sb;
  if (stat(dir_path, &sb)) {
    perror(dir_path);
    return 0;
  }

  if (S_ISDIR(sb.st_mode))
    return 1;

  return 0;
}

string DirMonitor::GetFullDirPath(const char *dir_name, int pwd) const
{
  const auto it = wd2p_map_.find(pwd);
  if (it == wd2p_map_.end()) {
    cout << "No dir path found for parent watch descriptor " << pwd << endl;
    return "";
  }
  return (string)(it->second + "/" + dir_name);
}


string DirMonitor::GetDirPath(int dir_wd) const
{
  const auto it = wd2p_map_.find(dir_wd);
  if (it == wd2p_map_.end()) {
    cout << "No dir path found for " << dir_wd << endl;
    return "";
  }
  return it->second;
}


int DirMonitor::GetWatchDescriptor(const string &dir_path) const
{
  const auto it = p2wd_map_.find(dir_path);
  if (it == p2wd_map_.end()) {
    cerr << "No watch descriptor found for " << dir_path
         << " adding a new watch " << endl;
    return -1;
  }
  return it->second;
}

int DirMonitor::GetParentWatchDescriptor(int dir_wd) const
{
  string path = GetDirPath(dir_wd);
  if (path.empty()) return -1;
  return GetParentWatchDescriptor(path, dir_wd);
}

int DirMonitor::GetParentWatchDescriptor(const string &path, int dir_wd) const
{
  size_t pos = path.find_last_of("/");
  // If there is no "/" in the path`
  if (pos == string::npos) {
    cerr << path << " has no parent " << endl;
    return -1;
  }
  return GetWatchDescriptor(path.substr(0, pos));
}

void DirMonitor::StorePair(const string &path, int wd)
{
  p2wd_map_[path] = wd;
  wd2p_map_[wd] = path;
}

void DirMonitor::RemovePair(const string &path, int wd)
{
  p2wd_map_.erase(path);
  wd2p_map_.erase(wd);
}

void DirMonitor::AddToParentList(int pwd, int wd)
{
  if (pwd == -1) return;
  wds_[pwd].insert(wd);
}


void DirMonitor::RemoveFromParentList(const string &path, int wd)
{
  RemoveFromParentList(GetParentWatchDescriptor(path, wd), wd);
}


void DirMonitor::RemoveFromParentList(int pwd, int wd)
{
  if (pwd == -1) return;
  wds_[pwd].erase(wd);
}

DirMonitor::DirMonitor(int argc, char *argv[]) : inotify_fd_(-1)
{
  inotify_fd_ = inotify_init();
  if (inotify_fd_ < 0) {
    perror("inotify_init");
    exit(1);
  }

  wds_.resize(MAX_USER_WATCHES);
  for (int i = 0; i < argc; ++i) {
    if (!is_dir(argv[i]))
      continue;

    string path(argv[i]);

    // Remove all the trailing "/" from the path
    path.erase(path.find_last_not_of("/") + 1);
    cout << "Adding new watch for path " << path << endl;

    if (AddWatchRecursively(path, -1) < 0) {
      cerr << "adding watch recursively for " << path << " failed" << endl;
      continue;
    }
  }
}

int DirMonitor::AddWatch(const string &path, int pwd, int events)
{
  int wd = inotify_add_watch(inotify_fd_, path.c_str(), events);
  if (wd < 0) {
    perror("inotify_add_watch failed");
    return -1;
  }
  StorePair(path, wd);
  AddToParentList(pwd, wd);
  return wd;
}

int DirMonitor::AddWatchRecursively(const string &dir_path, int pwd)
{
  DIR *dir;
  struct dirent *dir_entry;
  int dir_wd, fd;

  if (dir_path.empty()) {
    cerr << "Empty directory path for watching" << endl;
    return -1;
  }

  cout << "Add watch for directory: " << dir_path << endl;

  fd = open(dir_path.c_str(), O_NOFOLLOW | O_RDONLY);
  if (fd < 0) {
    perror("open failed");
    return -1;
  }

  dir = fdopendir(fd);
  if (!dir) {
    perror("opendir failed");
    close(fd);
    return -1;
  }

  dir_entry = readdir(dir);
  if (!dir_entry) {
    perror("readdir failed");
    closedir(dir);
    close(fd);
    return -1;
  }
  dir_wd = AddWatch(dir_path, pwd, INOTIFY_DIR_EVENTS);

  while (dir_entry) {
    // Skipping . and ..
    if (!strncmp(dir_entry->d_name, ".", strlen(dir_entry->d_name)) ||
        !strncmp(dir_entry->d_name, "..", strlen(dir_entry->d_name))) {
      dir_entry = readdir(dir);
      continue;
    }

    string full_path;
    full_path.append(dir_path);
    full_path.append("/");
    full_path.append(dir_entry->d_name);

    if (is_dir(full_path.c_str())) {
      if (AddWatchRecursively(full_path, dir_wd) < 0) {
        cerr << "Add failed for " << full_path << endl;
      }
    } else {
        AddWatch(full_path, dir_wd, INOTIFY_FILE_EVENTS);
    }
    dir_entry = readdir(dir);
  }

  closedir(dir);
  close(fd);

  return 1;
}

// Handler for the IN_CREATE event:
// Directory created in watched directory (*).
void DirMonitor::HandleDirectoryCreate(const char *name, int pwd)
{
  cout << "IN_CREATE triggered for directory " << name << " and parent wd "
       << pwd << endl;

  string full_path = GetFullDirPath(name, pwd);
  if (full_path.empty()) return;

  if (AddWatchRecursively(full_path, pwd) < 0) {
    cerr << "Add watch recursively for " << full_path << " failed" << endl;
  }
}

// Handler for the IN_CREATE event:
// File created in watched directory (*).
void DirMonitor::HandleFileCreate(const char *name, int pwd)
{
  cout << "IN_CREATE triggered for file " << name << " and parent wd "
       << pwd << endl;
  string full_path = GetFullDirPath(name, pwd);
  if (full_path.empty()) return;
  AddWatch(full_path, pwd, INOTIFY_FILE_EVENTS);
}

// Handler for the IN_MODIFY event:
// File modified in watched directory (*).
void DirMonitor::HandleFileModify(const char *name, int pwd)
{
  cout << "IN_MODIFY triggered for file " << name << " and parent wd "
       << pwd << endl;
}

// Handler for the IN_DELETE_SELF inotify event:
// Watched file/directory was itself deleted.
void DirMonitor::HandleDeleteSelf(int wd)
{
  cout << "IN_DELETE_SELF triggered for wd " << wd << endl;
  string path = GetDirPath(wd);
  if (path.empty()) return;
  RemovePair(path, wd);
  wds_[wd].clear();
  RemoveFromParentList(path, wd);
}

// Handler for the IN_MOVED_FROM event:
// File moved out of watched directory (*).
void DirMonitor::HandleMoveFrom(const char *name, int wd)
{
  cout << "IN_MOVED_FROM triggered for directory " << name << " and parent wd "
       << wd << endl;

  string full_path = GetFullDirPath(name, wd);
  if (full_path.empty()) return;

  int dir_wd = GetWatchDescriptor(full_path);
  if (dir_wd == -1) return;

  RemovePair(full_path, dir_wd);
  RemoveFromParentList(wd, dir_wd);
}

// Handler for the IN_MOVED_TO event:
// File moved into watched directory (*).
void DirMonitor::HandleMoveTo(const char *name, int wd)
{
  cout << "IN_MOVED_TO triggered for directory " << name << " and parent wd "
       << wd << endl;

  string full_path = GetFullDirPath(name, wd);
  if (full_path.empty()) return;

  if (AddWatchRecursively(full_path, wd) < 0) {
    cerr << " watch recursively for " << full_path << " failed" << endl;
  }
}

void DirMonitor::StartMonitoring()
{
  int len;
  char buf[MAX_EVENTS_BUF_SIZE], *ptr;
  struct inotify_event *event;

  while (1) {
    len = read(inotify_fd_, buf, sizeof(buf));
    if (len == 0) {
      cerr << "inotify read EOF" << endl;
      break;
    }
    if (len < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        cerr << "error EINTR or EAGAIN" << endl;
        continue;
      }
      return;
    }

    for (ptr = buf; ptr < buf + len; ptr += event->len + sizeof(*event)) {
      event = (struct inotify_event *)ptr;

      // Check to see if the the event struct is complete.
      if (ptr + sizeof(*event) > buf + len) {
        cerr << "inotify: incomplete inotify event" << endl;
        break;
      }
      if (event->mask & (IN_IGNORED | IN_UNMOUNT)) {
        //cerr << "non-existing " << event->name << " triggered event "
        //     << " for parent watch descriptor " << event->wd << endl;
        continue;
      } else if (event->mask & IN_Q_OVERFLOW) {
        cout << "Event queue overflowed" << endl;
        break;
      }

      //cout << "EVENT WD: " << event->wd << endl;
      //cout << "EVENT MASK: " << event->mask << endl;
      //cout << "EVENT COOKIE: " << event->cookie << endl;
      //cout << "EVENT LEN: " << event->len << endl;

      if (event->mask & IN_DELETE_SELF) {
        HandleDeleteSelf(event->wd);
      }

      if (event->mask & IN_ISDIR) {
        if (event->mask & IN_CREATE) {
          HandleDirectoryCreate(event->name, event->wd);
        }
        if (event->mask & IN_MOVED_FROM) {
          HandleMoveFrom(event->name, event->wd);
        }
        if (event->mask & IN_MOVED_TO) {
          HandleMoveTo(event->name, event->wd);
        }
      } else {
        if (event->mask & IN_CREATE) {
          HandleFileCreate(event->name, event->wd);
        }
        if (do_output_modify_events && (event->mask & (IN_MODIFY|IN_MOVE))) {
          HandleFileModify(event->name, event->wd);
        }
      }
    }
  }

}

DirMonitor::~DirMonitor() {
  close(inotify_fd_);
}


void usage(const char *progname)
{
  cerr << "usage: " << progname << " [-m] <dirname_1>...<dirname_n>" << endl;
  cerr << "\t-m: watch for file modification, not just creation" << endl;
  exit(1);
}


int main(int argc, char *argv[])
{
  int c;

  while ((c = getopt(argc, argv, "m")) != -1) {
    switch (c) {
    case 'm':
      do_output_modify_events = 1;
      break;
    case '?':
    default:
      usage(argv[0]);
      break;
    }
  }

  argc -= optind;
  if (argc < 1) {
    usage(argv[0]);
  }

  DirMonitor monitor(argc, &argv[optind]);
  monitor.StartMonitoring();
  return 0;
}

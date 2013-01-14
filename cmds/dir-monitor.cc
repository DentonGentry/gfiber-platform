#include "dir-monitor.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

using namespace std;

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

  cerr << dir_path << " is not a directory" << endl;
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
  cout << "Trying to find the watch descriptor for " << dir_path << endl;
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
  cout << "Trying to find parent watch descriptor for "
       << dir_wd << " and " << path << endl;

  size_t pos = path.find_last_of("/");
  // If there is no "/" in the path`
  if (pos == string::npos) {
    cerr << path << " has no parent " << endl;
    return -1;
  }
  cout << "Parent path " << path.substr(0, pos) << endl;
  return GetWatchDescriptor(path.substr(0, pos));
}

void DirMonitor::StorePair(const string &path, int wd)
{
  p2wd_map_[path] = wd;
  wd2p_map_[wd] = path;
  cout << "Will add to both maps pair (" << wd << ", " << path << ")" << endl;
}


void DirMonitor::RemovePair(const string &path, int wd)
{
  p2wd_map_.erase(path);
  wd2p_map_.erase(wd);
  cout << "Will remove from both maps pair (" << wd
       << ", " << path << ")" << endl;
}


void DirMonitor::AddToParentList(int pwd, int wd)
{
  if (pwd == -1) return;
  cout << "Will add " << wd << " to the list of " << pwd << endl;
  wds_[pwd].insert(wd);
}


void DirMonitor::RemoveFromParentList(const string &path, int wd)
{
  RemoveFromParentList(GetParentWatchDescriptor(path, wd), wd);
}


void DirMonitor::RemoveFromParentList(int pwd, int wd)
{
  if (pwd == -1) return;
  cout << "Will remove " << wd << " from the list of " << pwd << endl;
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
    cout << "New path " << path << endl;

    if (AddWatchRecursively(path, -1) < 0) {
      cerr << "adding watch recursively for " << path << " failed" << endl;
      continue;
    }
  }
}

int DirMonitor::AddWatch(const string &dir_path, int pwd)
{
  int dir_wd = inotify_add_watch(inotify_fd_, dir_path.c_str(),
                                 INOTIFY_EVENTS | IN_ONLYDIR);
  if (dir_wd < 0) {
    perror("inotify_add_watch failed");
    return -1;
  }
  StorePair(dir_path, dir_wd);
  AddToParentList(pwd, dir_wd);
  return dir_wd;
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

  cerr << "Trying to add directory: " << dir_path << endl;

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
  dir_wd = AddWatch(dir_path, pwd);

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

    if (is_dir(full_path.c_str()) &&
        AddWatchRecursively(full_path, dir_wd) < 0) {
      cerr << "Add failed for " << full_path << endl;
    }
    dir_entry = readdir(dir);
  }

  closedir(dir);
  close(fd);

  return 1;
}

/*
//TODO(irinams): remove this code if it turns out not to be useful
//any more
int DirMonitor::RemoveWatch(const string &dir_path, int dir_wd)
{
  if (dir_path.empty()) {
    cerr << "Invalid dir path for remove watch" << endl;
    return -1;
  }

  cout << "Will remove directory " << dir_path
       << " and watch descriptor " << dir_wd << endl;

  if (inotify_rm_watch(inotify_fd_, dir_wd) < 0) {
    perror("inotify_rm_watch failed");
    return -1;
  }
//TODO(irinams): remove this code if it turns out not to be useful
//any more
  RemoveFromParentList(dir_wd);
  RemovePair(dir_path, dir_wd);
  return 1;
}
*/

/*
//TODO(irinams): remove this code if it turns out not to be useful
//any more
int DirMonitor::RemoveWatchRecursively(int dir_wd)
{
  cerr << "Will remove watch_descriptor " << dir_wd << endl;
  for (set<int>::iterator it = wds_[dir_wd].begin();
       it != wds_[dir_wd].end(); ++it) {
    if (RemoveWatchRecursively(*it) < 0) {
      cerr << "RemoveWatchRecursively failed for wd " << *it << endl;
    }
  }

  string dir_path = GetDirPath(dir_wd, &dir_path);
  if (dir_path.empty()) return -1;

  if (RemoveWatch(dir_path, dir_wd) < 0) {
    perror("inotify_rm_watch failed");
    return -1;
  }

  wds_[dir_wd].clear();
  return 1;
}
*/


// Handler for the IN_CREATE event:
// File/directory created in watched directory (*).
void DirMonitor::HandleCreate(const char *name, int pwd)
{
  cout << "directory " << name << " triggered IN_CREATE for parent wd "
       << pwd << endl;

  string full_path = GetFullDirPath(name, pwd);
  if (full_path.empty()) return;

  if (AddWatchRecursively(full_path, pwd) < 0) {
    cerr << " watch recursively for " << full_path << " failed" << endl;
  }
}


// Handler for the IN_DELETE_SELF inotify event:
// Watched file/directory was itself deleted.
void DirMonitor::HandleDeleteSelf(int wd)
{
  cout << "IN_DELETE_SELF triggered for wd " << wd << endl;
  string path = GetDirPath(wd);
  if (path.empty()) return;
  cout << "\tdirectory " << path << endl;

  RemovePair(path, wd);
  wds_[wd].clear();

  RemoveFromParentList(path, wd);
}


// Handler for the IN_DELETE inotify event:
// File/directory deleted from watched directory (*).
void DirMonitor::HandleDelete(const char *name, int wd)
{
  cerr << "directory " << name << " triggered IN_DELETE for parent wd "
       << wd << endl;

  string full_path = GetFullDirPath(name, wd);
  if (full_path.empty()) return;

  int dir_wd = GetWatchDescriptor(full_path);
  if (dir_wd == -1) return;

  /*
  //TODO(irinams): remove this code
  // If a folder has been deleted, the watch was automatically removed too
  if (RemoveWatch(full_path, dir_wd) < 0) {
    cerr << "RemoveWatch for " << full_path << " and "
      << dir_wd << " failed " << endl;
  }
  */
  RemovePair(full_path, dir_wd);
  RemoveFromParentList(wd, dir_wd);
}

// Handler for the IN_MOVED_FROM event:
// File moved out of watched directory (*).
void DirMonitor::HandleMoveFrom(const char *name, int wd)
{
  cerr << "directory " << name << " triggered IN_MOVED_FROM for parent wd "
       << wd << endl;

  string full_path = GetFullDirPath(name, wd);
  if (full_path.empty()) return;

  int dir_wd = GetWatchDescriptor(full_path);
  if (dir_wd == -1) return;

  /*
  //TODO(irinams): remove this code
  // If a folder has been deleted, the watch was automatically removed too
  // and all it's subdirectories
  if (RemoveWatch(full_path, dir_wd) < 0) {
    cerr << "RemoveWatch for " << full_path << " and "
      << dir_wd << " failed " << endl;
  }
  */
  RemovePair(full_path, dir_wd);
  RemoveFromParentList(wd, dir_wd);
}

// Handler for the IN_MOVED_TO event:
// File moved into watched directory (*).
void DirMonitor::HandleMoveTo(const char *name, int wd)
{
  cerr << "directory " << name << " triggered IN_MOVED_TO for parent wd "
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
      // Check to see if the the event struct is not incomplete.
      if (ptr + sizeof(*event) > buf + len) {
        cerr << "inotify: incomplete inotify event" << endl;
        break;
      }
      if (event->mask & (IN_IGNORED | IN_UNMOUNT)) {
        //cerr << "non-existing " << event->name << " triggered event "
        //     << " for parent watch descriptor " << event->wd << endl;
        continue;
      } else if (event->mask & IN_Q_OVERFLOW) {
        cerr << "inotify: event queue overflowed" << endl;
        break;
      }

      cout << "EVENT WD: " << event->wd << endl;
      cout << "EVENT MASK: " << event->mask << endl;
      cout << "EVENT COOKIE: " << event->cookie << endl;
      cout << "EVENT LEN: " << event->len << endl;

      if (event->mask & IN_DELETE_SELF) {
        HandleDeleteSelf(event->wd);
      }
      if (event->mask & IN_CREATE) {
        HandleCreate(event->name, event->wd);
      }
      //if (event->mask & IN_DELETE) {
      //  HandleDelete(event->name, event->wd);
      //}
      if (event->mask & IN_MOVED_FROM) {
        HandleMoveFrom(event->name, event->wd);
      }
      if (event->mask & IN_MOVED_TO) {
        HandleMoveTo(event->name, event->wd);
      }
      //if (event->len && ptr + sizeof(*event) + event->len <= buf + len) {
      // Pathname is null terminated.
      //  cout << "***********" << event->name << endl;
      //}
    }
  }

}

DirMonitor::~DirMonitor() {
  close(inotify_fd_);
}


int main(int argc, char *argv[])
{
  if (argc < 2) {
    cerr << "usage: " << argv[0] << " <dirname_1>...<dirname_n>" << endl;
    exit(1);
  }
  DirMonitor monitor(argc - 1, argv + 1);
  monitor.StartMonitoring();
  return 0;
}

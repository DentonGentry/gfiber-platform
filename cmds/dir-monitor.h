#ifndef INOTIFY_DIR_MONITOR_H_
#define INOTIFY_DIR_MONITOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>


#define INOTIFY_EVENTS IN_MOVE|IN_CREATE|IN_MODIFY|IN_DELETE_SELF

#define MAX_EVENTS_BUF_SIZE 65536
// TODO(irinams) : this value is hardcoded for the moment, it actually should
// be read from /proc/sys/fs/inotify/max_user_watches
#define MAX_USER_WATCHES 8192

class DirMonitor {
public:
  DirMonitor(int argc, char *argv[]);
  ~DirMonitor();

  void StartMonitoring();

  int AddWatch(const std::string &dir_path, int root_dir_wd);
  int AddWatchRecursively(const std::string &dir_path, int root_dir_wd);

  //int RemoveWatch(const std::string  &dir_path, int dir_wd);
  //int RemoveWatchRecursively(int dir_wd);

  void HandleCreate(const char *name, int wd);
  void HandleDelete(const char *name, int wd);
  void HandleMoveFrom(const char *name, int wd);

  void HandleMoveTo(const char *name, int wd);
  void HandleDeleteSelf(int wd);

private:

  // Disabling no arguments constructor and copy-constructor
  DirMonitor();
  DirMonitor(const DirMonitor &monitor);

  void StorePair(const std::string &path, int wd);
  void RemovePair(const std::string &path, int wd);
  void AddToParentList(int pwd, int wd);
  void RemoveFromParentList(const std::string &path, int wd);
  void RemoveFromParentList(int pwd, int wd);

  void GetFullDirPath(const char *dir_name, int pwd, std::string *path) const;
  void GetDirPath(int dir_wd, std::string *path) const;
  std::string GetFullDirPath(const char *dir_name, int pwd) const;
  std::string GetDirPath(int dir_wd) const;

  int GetWatchDescriptor(const std::string &dir_path) const;
  int GetParentWatchDescriptor(int dir_wd) const;
  int GetParentWatchDescriptor(const std::string &path, int dir_wd) const;

  typedef std::map<std::string, int> Path2WDMap;
  typedef std::map<int, std::string> WD2PathMap;
  typedef std::vector<std::set<int> > WDChildrenList;

  Path2WDMap p2wd_map_; // path to watch descriptor map
  WD2PathMap wd2p_map_; // watch descriptor to path map
  WDChildrenList wds_;

  int inotify_fd_;
};


#endif  // INOTIFY_DIR_MONITOR_H_

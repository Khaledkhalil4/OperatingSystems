#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#include <vector>
#include <string.h>
#include <stack>


#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

void setAlarm();


class Command {
private:
    friend std::ostream& operator<<(std::ostream& os, const Command& cmd);
public:
    std::string m_cmd_line;
    char** m_args;
    int m_argCnt;
    bool m_isBackground;
    bool m_isAlarm;
    time_t m_alarmDuration;

  Command(const char* cmd_line, bool isAlarm = false, time_t alarmDuration = 0);
  virtual ~Command();
  virtual void execute() = 0;
  std::string getCommand();
};

class BuiltInCommand : public Command {
 public:
    bool m_isAlarm;
    pid_t m_pid;
    int m_duration;

  BuiltInCommand(const char* cmd_line, bool isTimeout = false);
  virtual ~BuiltInCommand() = default;
  void updateAlarm();
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() = default;
  void execute() override;
};

class PipeCommand : public Command {
  std::string m_command1;
  std::string m_command2;
  std::string m_pipeType;

 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() = default;
  void execute() override;
};

class RedirectionCommand : public Command {
 std::string m_command;
 std::string m_redirectionType;
 std::string m_fileName;
 int m_fd;
 int m_stdout;
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() = default;
  void execute() override;
  bool prepare();
  void cleanup();
};

class ChangeDirCommand : public BuiltInCommand {
    char** m_plastPwd;
public:
  ChangeDirCommand(const char* cmd_line, char** plastPwd);
  virtual ~ChangeDirCommand() = default;
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() = default;
  void execute() override;
};

class ChPromptCommand : public BuiltInCommand {
public:
    ChPromptCommand(const char* cmd_line);
    virtual ~ChPromptCommand() = default;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() = default;
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
    JobsList* m_jobsList;
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() = default;
  void execute() override;
};


class JobsList {
 public:
  class JobEntry {
  public:
      int m_jobId;
      std::string m_cmd;
      time_t m_startTime;
      bool m_isStopped;
      pid_t m_pid;

      friend std::ostream& operator<<(std::ostream& os, const JobEntry& jobEntry);
  public:
      JobEntry(int jobId, std::string cmd, bool isStopped, pid_t pid);
      ~JobEntry();
      pid_t getPid() const;
  };

  std::vector<JobEntry> m_jobsList;
  int m_maxJobId;
 public:
  JobsList();
  ~JobsList() = default;
  void addJob(std::string cmd, pid_t pid, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
};

class AlarmList {
public:
    class AlarmEntry {
    public:
        pid_t m_pid;
        std::string m_cmd;
        time_t m_startTime;
        time_t m_duration;
        time_t m_finishTime;
        AlarmEntry(pid_t pid, std::string cmd, time_t duration);
        ~AlarmEntry() = default;
    };
    std::vector<AlarmEntry> m_alarmList;
    AlarmList();
    ~AlarmList() = default;
    void addAlarmEntry(std::string command, time_t duration, pid_t pid);
    void deleteAlarms();
};

class JobsCommand : public BuiltInCommand {
 JobsList* m_jobs;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 JobsList* m_jobsList;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList* m_jobsList;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
public:
  explicit TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
};

class ChmodCommand : public BuiltInCommand {
 public:
  ChmodCommand(const char* cmd_line);
  virtual ~ChmodCommand() {}
  void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand {
  std::string m_path;
 public:
  GetFileTypeCommand(const char* cmd_line);
  virtual ~GetFileTypeCommand() {}
  void execute() override;
};

class SetcoreCommand : public BuiltInCommand {
  // TODO: Add your data members
 public:
  explicit SetcoreCommand(const char* cmd_line);
  virtual ~SetcoreCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 JobsList* m_jobsList;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class SmallShell {
 private:
  std::string m_prompt;
  bool m_prevDirFlag;
  char *m_prevDir;


  SmallShell();
 public:
  JobsList* m_jobsList;
  pid_t m_CurrentPid;
  std::string m_currentCommand;
  bool m_lastCommandForeGround;
  int m_foregroundedCommandJobId;
  AlarmList* m_alarmsList;
  bool m_alarmInForeground;
  time_t m_foregroundStartTime;
  time_t m_foregroundDuration;
  time_t m_foregroundFinishTime;

  Command *CreateCommand(const char* cmd_line, bool isAlarm = false);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char *cmd_line, bool isAlarm = false, time_t duration = time(nullptr));
  void setPrompt(const std::string& prompt);
  std::string getPrompt();
  char **getPrevDir();
  void prevDirChanged();
  bool prevDirExists() const;
};

#endif //SMASH_COMMAND_H_

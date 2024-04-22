#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <sched.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const int MAX_PATH_LEN = 200;
const int MAX_COMMAND_LENGTH = 200;
const int MAX_ARGS_CNT = 20;

void printLineError(const char* errorMessage, bool printAngleBrackets);
char* getPath();
bool isNumber(const char *str, bool kill);
bool containsWildCard(std::string cmd_line);
void printErrorMessage(std::string msg);
void removeChar(std::string& str, char c);


string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


SmallShell::SmallShell() : m_prompt("smash"), m_prevDirFlag(false), m_prevDir(nullptr), m_jobsList(new JobsList()),
                                              m_CurrentPid(-1), m_currentCommand(""), m_lastCommandForeGround(false),
                                              m_foregroundedCommandJobId (-1), m_alarmsList(new AlarmList()),
                                              m_alarmInForeground(false), m_foregroundStartTime(time(nullptr)),
                                              m_foregroundDuration(time(nullptr)), m_foregroundFinishTime(time(nullptr))
                                              {}

SmallShell::~SmallShell() {
    delete m_jobsList;
    delete m_alarmsList;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
**/

Command * SmallShell::CreateCommand(const char* cmd_line, bool isAlarm) {
  string cmd_s = _trim(string(cmd_line));
  if(cmd_s.find_first_not_of(WHITESPACE) == string::npos)
  {
      return nullptr;
  }
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  m_lastCommandForeGround = false;
  m_alarmInForeground = false;

  if(isAlarm)
  {
      std::string command(cmd_s);
      command = command.substr(command.find_first_of(" \n") + 1, command.size());
      command = command.substr(command.find_first_of(" \n") + 1, command.size());
      firstWord = cmd_s.substr(0, command.find_first_of(" \n"));
  }

  if(cmd_s.find(">") != string::npos || cmd_s.find(">>") != string::npos)
  {
      return new RedirectionCommand(cmd_line);
  }
  if(cmd_s.find("|") != string::npos || cmd_s.find("|&") != string::npos)
  {
        return new PipeCommand(cmd_line);
  }
  if(firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0) {
      return new ChPromptCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0){
      return new ShowPidCommand(cmd_line);
  } else if (firstWord.compare("cd") == 0 || firstWord.compare("cd&") == 0){
      return new ChangeDirCommand(cmd_line, SmallShell::getPrevDir());
  } else if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0) {
        return new GetCurrDirCommand(cmd_line);
  }
     else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0) {
        return new JobsCommand(cmd_line,m_jobsList);
  }
      else if (firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0) {
       return new ForegroundCommand(cmd_line,m_jobsList);
  }
      else if (firstWord.compare("bg") == 0 || firstWord.compare("bg&") == 0) {
        return new BackgroundCommand(cmd_line, m_jobsList);
  }
      else if(firstWord.compare("kill") == 0 || firstWord.compare("kill&") == 0) {
          return new KillCommand(cmd_line, m_jobsList);
      }
        else if (firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0) {
           return new  QuitCommand(cmd_line,m_jobsList);
  }
  else if (firstWord.compare("chmod") == 0 ) {
      return new  ChmodCommand(cmd_line);
  }
  else if (firstWord.compare("setcore") == 0 ) {
      return new SetcoreCommand(cmd_line);
  }
  else if (firstWord.compare("getfileinfo") == 0) {
      return new GetFileTypeCommand(cmd_line);
  }
  else if (firstWord.compare("timeout") == 0) {
      return new TimeoutCommand(cmd_line);
  }
  else {
      return new ExternalCommand(cmd_line);
  }
}

void SmallShell::executeCommand(const char *cmd_line, bool isAlarm, time_t duration) {
  Command* cmd;
  if(!isAlarm)
      cmd = CreateCommand(cmd_line);
  else
      cmd = CreateCommand(cmd_line, true);

  if(cmd == nullptr)
      return;
  if(isAlarm)
  {
      cmd->m_alarmDuration = duration;
      cmd->m_isAlarm = isAlarm;
  }


  SmallShell::getInstance().m_jobsList->removeFinishedJobs();
  cmd->execute();
  m_currentCommand = "";
  m_CurrentPid = -1;
  delete cmd;
}

char **SmallShell::getPrevDir() {
    return &m_prevDir;
}

void SmallShell::prevDirChanged() {
    m_prevDirFlag = true;
}

bool SmallShell::prevDirExists() const {
    return m_prevDirFlag;
}

std::string SmallShell::getPrompt() {
    return m_prompt;
}

void SmallShell::setPrompt(const std::string& prompt) {
    m_prompt = prompt;
}

Command::Command(const char *cmd_line, bool isAlarm, time_t alarmDuration) : m_cmd_line(cmd_line), m_isAlarm(isAlarm),
                                                                             m_alarmDuration(alarmDuration)
{
    m_args = new char*[MAX_ARGS_CNT]();
    m_argCnt = _parseCommandLine(cmd_line, m_args);
    if(_isBackgroundComamnd(cmd_line))
        m_isBackground = true;
    else
        m_isBackground = false;
}

Command::~Command() {
    for(int i = 0; i < m_argCnt; i++)
    {
        if(m_args[i] != NULL)
            free(m_args[i]);
    }
    delete[] m_args;
    //Might cause leaks, check later.
}

std::ostream &operator<<(ostream &os, const Command &cmd) {
    os << cmd.m_cmd_line;
    return os;
}

std::string Command::getCommand() {
    std::string cmd("");
    if(m_isAlarm)
    {
        cmd.append("timeout ");
        cmd.append(std::to_string((int)m_alarmDuration));
        cmd.append(" ");
    }
    cmd.append(m_cmd_line);
    return cmd;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line, bool isTimeout) :
                                                        Command(cmd_line) {
    char copy[MAX_COMMAND_LENGTH];
    strcpy(copy, cmd_line);
    if(_isBackgroundComamnd(copy) && !isTimeout)
        _removeBackgroundSign(copy);
    m_cmd_line = copy;

}

void BuiltInCommand::updateAlarm() {
    if(m_isAlarm)
    {
        SmallShell::getInstance().m_alarmsList->addAlarmEntry(m_cmd_line, m_duration, m_pid);
        setAlarm();
    }
}

ChPromptCommand::ChPromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChPromptCommand::execute() {
    if(m_argCnt == 1)
    {
        SmallShell::getInstance().setPrompt("smash");
    }
    else
    {
        SmallShell::getInstance().setPrompt(m_args[1]);
    }
    updateAlarm();
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    updateAlarm();
    std::cout << "smash pid is " << getpid() << std::endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    updateAlarm();
    std::cout << getPath() << std::endl;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line),
                                                                            m_plastPwd(plastPwd) {}

void ChangeDirCommand::execute() {
    updateAlarm();
    if(m_argCnt > 2)
    {
        printErrorMessage("smash error: cd: too many arguments");
        return;
    }
    char *tmp = getPath();
    if(m_argCnt == 2 && strcmp(m_args[1], "-") == 0)
    {
        if(!SmallShell::getInstance().prevDirExists())
        {
            
            printErrorMessage("smash error: cd: OLDPWD not set");
            return;
        }
        if(chdir(*m_plastPwd) == -1)
        {
            perror("smash error: chdir failed");
            return;
        }
    }
    else if(chdir(m_args[1]) == -1)
    {
        
        perror("smash error: chdir failed");
        return;
    }
    *m_plastPwd = tmp;
    SmallShell::getInstance().prevDirChanged();
}

void printLineError(const char* errorMessage, bool printAngleBrackets) {
    std::string msg = "smash error:";
    if(printAngleBrackets)
        msg.append(">");
    msg.append(" ");
    msg.append(errorMessage);
    
    printErrorMessage(msg.c_str());
}

char* getPath() {
    char *cwd = new char[MAX_PATH_LEN];
    getcwd(cwd, MAX_PATH_LEN);
    return cwd;
}


JobsList::JobsList() : m_jobsList(), m_maxJobId(0){

}

void JobsList::addJob(std::string cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    SmallShell &smash = SmallShell::getInstance();
    if(!smash.m_lastCommandForeGround)
    {
        JobEntry job(++m_maxJobId, cmd, isStopped, pid);
        m_jobsList.emplace_back(job);
    }
    else
    {
        if(smash.m_foregroundedCommandJobId <= 0)
        {
            printErrorMessage("smash error: no foregrounded job");
            return;
        }
        JobEntry job(smash.m_foregroundedCommandJobId, cmd, isStopped, pid);
        bool inserted = false;
        for(auto it = m_jobsList.begin(); it != m_jobsList.end(); ++it)
        {
            if(it->m_jobId > job.m_jobId)
            {
                m_jobsList.insert(it, job);
                inserted = true;
                break;
            }
        }
        if(!inserted)
            m_jobsList.emplace_back(job);
    }
}

void JobsList::printJobsList() {
    for(JobEntry& job : m_jobsList)
    {
        std::cout << job;
    }
}

JobsList::JobEntry::JobEntry(int jobId, std::string cmd, bool isStopped, pid_t pid) : m_jobId(jobId), m_cmd(cmd),
                                                                        m_startTime(std::time(nullptr)),
                                                                        m_isStopped(isStopped),m_pid(pid) {}

pid_t JobsList::JobEntry::getPid() const {
    return m_pid;
}

std::ostream &operator<<(ostream &os, const JobsList::JobEntry &jobEntry) {
    os << "[" << jobEntry.m_jobId << "] " << jobEntry.m_cmd << " : " << jobEntry.getPid() << " ";
    os << std::difftime(std::time(nullptr), jobEntry.m_startTime) << " secs";
    if(jobEntry.m_isStopped)
        os << " (stopped)";
    os << std::endl;
    return os;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for(JobEntry& job : m_jobsList)
    {
        if(job.m_jobId == jobId)
            return &job;
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    int newMax = 0;
    for(auto it = m_jobsList.begin(); it != m_jobsList.end(); ++it)
    {
        JobEntry job = *it;
        if(job.m_jobId == jobId)
        {
            m_jobsList.erase(it);
            break;
        }
    }

    for(auto it = m_jobsList.begin(); it != m_jobsList.end(); ++it)
    {
        JobEntry job = *it;
        if(job.m_jobId > newMax && job.m_jobId != jobId)
            newMax = job.m_jobId;
    }
    m_maxJobId = newMax;
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if(!m_jobsList.empty())
    {
        *lastJobId = m_jobsList.back().m_jobId;
        return &m_jobsList.back();
    }
    return nullptr;
}

void JobsList::killAllJobs() {
    for(JobEntry &job : m_jobsList)
    {
        if(kill(job.getPid(), SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
    }
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    for(auto it = m_jobsList.rbegin(); it != m_jobsList.rend(); ++it)
    {
        if(it->m_isStopped)
        {
            *jobId = it->m_jobId;
            return &(*it);
        }
    }
    return nullptr;
}

void JobsList::removeFinishedJobs() {
    for(auto it = m_jobsList.begin(); it != m_jobsList.end(); ++it)
    {
        auto job = *it;
        int res = waitpid(job.getPid(), NULL, WNOHANG);
        if(res == -1 || res == job.getPid())
        {
            m_jobsList.erase(it);
            --it;
        }
    }
    int newMax = 0;
    for(auto it = m_jobsList.begin(); it != m_jobsList.end(); ++it)
    {
        auto job = *it;
        if(job.m_jobId > newMax)
            newMax = job.m_jobId;
    }
    m_maxJobId = newMax;
}


ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line),
                                                                             m_jobsList(jobs) {}

void ForegroundCommand::execute() {
    updateAlarm();
    if(m_argCnt > 2 || m_argCnt < 1 || (m_argCnt == 2 && !isNumber(m_args[1], false)))
    {
        printLineError("fg: invalid arguments", false);
        return;
    }
    int jobId = 0;
    int jobPid = 0;
    JobsList::JobEntry* job = nullptr;
    if(m_argCnt == 1)
    {
        job = m_jobsList->getLastJob(&jobId);
        if(job == nullptr)
        {
            printLineError("fg: jobs list is empty", false);
            return;
        }

    }
    if(m_argCnt == 2)
    {
        jobId = stoi(m_args[1]);
        job = m_jobsList->getJobById(jobId);
        if(job == nullptr)
        {
            std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
    }
    jobPid = job->getPid();
    std::cout << job->m_cmd << " : " << jobPid << std::endl;
    if(job->m_isStopped)
    {
        if (kill(jobPid, SIGCONT))
        {
            perror("smash error: kill failed");
            return;
        }
    }
    SmallShell &smash = SmallShell::getInstance();
    smash.m_CurrentPid = jobPid;
    smash.m_currentCommand = job->m_cmd;
    smash.m_lastCommandForeGround = true;
    smash.m_foregroundedCommandJobId = jobId;
    m_jobsList->removeJobById(jobId);
    if(waitpid(jobPid, nullptr, WUNTRACED) == -1)
    {
        perror("smash error: waitpid failed");
        return;
    }

}

bool isNumber(const char *str, bool kill) {
   int num;
   try {
       num = stoi(str);
   } catch (...) {
       return false;
   }
   if(kill && num >= 0)
       return false;
   return true;
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {

}

void JobsCommand::execute() {
    updateAlarm();
    if(m_jobs != nullptr)
    {
        m_jobs->removeFinishedJobs();
        m_jobs->printJobsList();
    }
}

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobsList(jobs) {

}

void BackgroundCommand::execute() {
    updateAlarm();
    if(m_argCnt > 2 || m_argCnt < 1 || (m_argCnt == 2 && !isNumber(m_args[1], false)))
    {
        printLineError("bg: invalid arguments", false);
        return;
    }
    int jobId = 0;
    JobsList::JobEntry* job;
    if(m_argCnt == 1)
    {
        job = m_jobsList->getLastStoppedJob(&jobId);
        if(job == nullptr)
        {
            printLineError("bg: there is no stopped jobs to resume", false);
            return;
        }
    }
    if(m_argCnt == 2)
    {
        jobId = stoi(m_args[1]);
        job = m_jobsList->getJobById(jobId);
        if(job == nullptr)
        {
            std::cerr << "smash error: bg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
        if(!job->m_isStopped)
        {
            std::cerr << "smash error: bg: job-id " << jobId << " is already running in the background" << std::endl;
            return;
        }
    }
    std::cout << job->m_cmd << " : " << job->getPid() << std::endl;
    job->m_isStopped = false;
    if(kill(job->getPid(), SIGCONT))
    {
        perror("smash error: kill failed");
        return;
    }
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobsList(jobs) {

}

void QuitCommand::execute() {
    updateAlarm();
    if(m_argCnt >= 2 && strcmp(m_args[1], "kill") == 0)
    {
        std::cout << "smash: sending SIGKILL signal to " << m_jobsList->m_jobsList.size() << " jobs:" << std::endl;
        for(JobsList::JobEntry& job : m_jobsList->m_jobsList)
        {
            std::cout << job.getPid() << ": " << job.m_cmd << std::endl;
        }
        m_jobsList->killAllJobs();
    }
    std::exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), m_jobsList(jobs) {
}

void KillCommand::execute() {
    updateAlarm();
    if(m_argCnt != 3 || !isNumber(m_args[1], true) || !isNumber(m_args[2], false))
    {
        printLineError("kill: invalid arguments", false);
        return;
    }
    int jobId = stoi(m_args[2]);
    int signal = stoi(m_args[1] + 1);
    struct sigaction sigAct;
    if (sigaction(signal, nullptr, &sigAct) == -1)
    {
        printLineError("kill: invalid arguments", false);
        return;
    }
    JobsList::JobEntry* job = m_jobsList->getJobById(jobId);
    if(job == nullptr)
    {
        std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
        return;
    }
    std::cout << "signal number " << signal << " was sent to pid " << job->getPid() << std::endl;
    if(kill(job->getPid(), signal) == -1)
    {
        printLineError(m_cmd_line.c_str(), true);
        return;
    }
    if(signal == SIGSTOP)
        job->m_isStopped = true;
    if(signal == SIGCONT)
        job->m_isStopped = false;
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {
}

void ExternalCommand::execute() {
    std::string command(m_cmd_line);
    if(m_isAlarm)
    {
        std::string secondArg = m_args[1];
        command = m_cmd_line.substr(m_cmd_line.find(secondArg) + secondArg.size() + 1, m_cmd_line.size());
    }

    removeChar(command, '"'); // TODO: check if to keep. waseem
    char new_cmd[MAX_COMMAND_LENGTH];
    strcpy(new_cmd, command.c_str());
    if(m_isBackground)
        _removeBackgroundSign(new_cmd);
    char* argv[MAX_ARGS_CNT];
    _parseCommandLine(new_cmd, argv);
    pid_t pid = fork();
    if(pid == 0)
    {
        if(setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            exit(0);
        }
        if(containsWildCard(m_cmd_line))
        {
            char exe[] = "/bin/bash";
            char flag[] = "-c";

            char* const args[4] = {exe, flag, new_cmd, nullptr};
            if(execv(args[0], args) == -1)
            {
                perror("smash error: exec failed");
                exit(0);
            }
        }
        else
        {
            if(execvp(argv[0], argv) == -1)
            {
                perror("smash error: exec failed");
                exit(0);
            }
        }
        exit(0);
    }
    else
    {
        SmallShell& smash = SmallShell::getInstance();
        if(m_isBackground)
        {

            smash.m_jobsList->addJob(m_cmd_line, pid);
            if(m_isAlarm)
            {
                smash.m_alarmsList->addAlarmEntry(m_cmd_line, m_alarmDuration, pid);
                setAlarm();
            }
        }
        else
        {
            smash.m_CurrentPid = pid;
            smash.m_currentCommand = m_cmd_line;
            if(m_isAlarm)
            {
                smash.m_alarmInForeground = true;
                smash.m_foregroundStartTime = time(nullptr);
                smash.m_foregroundDuration = m_alarmDuration;
                smash.m_foregroundFinishTime = smash.m_foregroundStartTime + m_alarmDuration;
                setAlarm();
            }

            if(waitpid(pid, nullptr, WUNTRACED) == -1)
            {
                perror("smash error: waitpid failed");
                return;
            }
        }
    }
}

bool containsWildCard(std::string cmd_line) {
    if(cmd_line.find('?') != string::npos || cmd_line.find('*') != string::npos)
        return true;
    return false;
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    char command[MAX_COMMAND_LENGTH];
    strcpy(command, cmd_line);
    if(_isBackgroundComamnd(command))
        _removeBackgroundSign(command);
    m_cmd_line = command;
    int index = 0;
    if(m_cmd_line.find("|&") != string::npos)
    {
        index = (int)m_cmd_line.find("|&");
        m_pipeType = "|&";
        m_command1 = m_cmd_line.substr(0, index);
        m_command2 = m_cmd_line.substr(index + 2, m_cmd_line.size());
    }
    else
    {
        index = (int)m_cmd_line.find("|");
        m_pipeType = "|";
        m_command1 = m_cmd_line.substr(0, index);
        m_command2 = m_cmd_line.substr(index + 1, m_cmd_line.size());
    }

}

void PipeCommand::execute() {
    int my_pipe[2];
    SmallShell& smash = SmallShell::getInstance();
    if(pipe(my_pipe) == -1)
    {
        perror("smash error: pipe failed");
        return;
    }
    pid_t pid = fork();
    if(pid == -1)
    {
        if (close(my_pipe[0]) == -1)
        {
            perror("smash error: close failed");
        }
        if (close(my_pipe[1]) == -1)
        {
            perror("smash error: close failed");
        }
        perror("smash error: fork failed");
        return;
    }

    if(pid == 0)
    {
        if(setpgrp() == -1)
        {
            if (close(my_pipe[0]) == -1)
            {
                perror("smash error: close failed");
            }
            if (close(my_pipe[1]) == -1)
            {
                perror("smash error: close failed");
            }

            perror("smash error: setprgp failed");
            //return;
            exit(0);
        }
        if(close(my_pipe[1]) == -1)
        {
            if(close(my_pipe[0]) == -1)
            {
                perror("smash error: close failed");
            }
            perror("smash error: close failed");
            //return;
            exit(0);
        }
        if(dup2(my_pipe[0], STDIN_FILENO) == -1)
        {
            if(close(my_pipe[0]) == -1)
            {
                perror("smash error: close failed");
            }
            perror("smash error: dup2 failed");
            //return;
            exit(0);
        }
        smash.executeCommand(m_command2.c_str());
        if(close(my_pipe[0]) == -1)
        {
            perror("smash error: close failed");
            //return;
            exit(0);
        }

        if(close(STDIN_FILENO) == -1)
        {
            perror("smash error: close failed");
            //return;
            exit(0);
        }
        exit(0);
    }
    else
    {
        if(close(my_pipe[0]) == -1)
        {
            if(close(my_pipe[1]) == -1)
            {
                perror("smash error: close failed");
            }
            perror("smash error: close failed");
            return;
        }
        int fileFD;
        int stdChannel;
        if(strcmp(m_pipeType.c_str(), "|") == 0)
            stdChannel = STDOUT_FILENO;
        else
            stdChannel = STDERR_FILENO;
        fileFD = dup(stdChannel);
        if(fileFD == -1)
        {
            if(close(my_pipe[1]) == -1)
            {
                perror("smash error: close failed");
            }
            perror("smash error: dup failed");
            return;
        }
        if(dup2(my_pipe[1], stdChannel) == -1)
        {
            if(close(my_pipe[1]) == -1)
            {
                perror("smash error: close failed");
            }
            perror("smash error: dup2 failed");
            return;
        }

        smash.executeCommand(m_command1.c_str());

        if(close(my_pipe[1]) == -1)
        {
            perror("smash error: close failed");
            return;
        }
        if(close(stdChannel) == -1)
        {
            perror("smash error: close failed");
            return;
        }
        if(dup(fileFD) == -1)
        {
            perror("smash error: dup failed");
            return;
        }
        if(close(fileFD) == -1)
        {
            perror("smash error: close failed");
            return;
        }

        waitpid(pid, nullptr, 0);
    }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line), m_fd(0), m_stdout(0)
{
    char command[MAX_COMMAND_LENGTH];
    strcpy(command, cmd_line);
    if(_isBackgroundComamnd(command))
        _removeBackgroundSign(command);
    m_cmd_line = command;
    int index = 0;
    if(m_cmd_line.find(">>") != string::npos)
    {
        index = (int)m_cmd_line.find(">>");
        m_redirectionType = ">>";
        m_command = m_cmd_line.substr(0, index);
        m_fileName = m_cmd_line.substr(index + 2, m_cmd_line.size() - (index + 2));
        m_fileName = _trim(m_fileName);
    }
    else
    {
        index = (int)m_cmd_line.find(">");
        m_redirectionType = ">";
        m_command = m_cmd_line.substr(0, index);
        m_fileName = m_cmd_line.substr(index + 1, m_cmd_line.size() - (index + 1));
        m_fileName = _trim(m_fileName);
    }

}

void RedirectionCommand::execute()
{
    if(!prepare())
        return;
    SmallShell& smash = SmallShell::getInstance();
    smash.executeCommand(m_command.c_str());

    cleanup();
}

bool RedirectionCommand::prepare()
{
    m_stdout = dup(STDOUT_FILENO);
    if(m_stdout == -1)
    {
        perror("smash error: dup failed");
        return false;
    }

    if(close(STDOUT_FILENO) == -1)
    {
        perror("smash error: close failed");
        return false;
    }
    if(strcmp(m_redirectionType.c_str(), ">") == 0)
    {
        m_fd = open(m_fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0655);
        if(m_fd == -1)
        {
            if(dup2(m_stdout, STDOUT_FILENO) == -1)
            {
                perror("smash error: dup2 failed");
                return false;
            }
            perror("smash error: open failed");
            return false;
        }
    }
    else
    {
        m_fd = open(m_fileName.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0655);
        if(m_fd == -1)
        {
            if(dup2(m_stdout, STDOUT_FILENO) == -1)
            {
                perror("smash error: dup2 failed");
                return false;
            }
            perror("smash error: open failed");
            return false;
        }
    }
    return true;
}

void RedirectionCommand::cleanup()
{
    if(close(m_fd) == -1)
    {
        perror("smash error: close failed");
        return;
    }
    if(dup2(m_stdout, STDOUT_FILENO) == -1)
    {

        perror("smash error: dup failed");
        return;
    }
    if(close(m_stdout) == -1)
    {
        perror("smash error: close failed");
        return;
    }
}

GetFileTypeCommand::GetFileTypeCommand(const char *cmd_line) : BuiltInCommand(cmd_line), m_path()
{
}

void GetFileTypeCommand::execute()
{
    // The path and size may be printed wrong
    updateAlarm();
    if(m_argCnt != 2)
    {
        printLineError("gettype: invalid arguments", false);
        return;
    }
    m_path = m_args[1];
    struct stat fileInfo;
    if(stat(m_path.c_str(), &fileInfo) != 0)
    {

        perror("smash error: stat failed");
        return;
    }
    std::string fileType;
    off_t fileSize = fileInfo.st_size;
    switch (fileInfo.st_mode & S_IFMT) {
        case S_IFREG:
            fileType = "regular file";
            break;
        case S_IFDIR:
            fileType = "directory";
            break;
        case S_IFCHR:
            fileType = "character device";
            break;
        case S_IFBLK:
            fileType = "block device";
            break;
        case S_IFIFO:
            fileType = "FIFO";
            break;
        case S_IFLNK:
            fileType = "symbolic link";
            break;
        case S_IFSOCK:
            fileType = "socket";
            break;
        default:
            fileType = "not found";
    }

    std::cout << m_path << "'s type is \"" << fileType << "\" and takes up " << fileSize << " bytes" << std::endl;
}

ChmodCommand::ChmodCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {

}

void ChmodCommand::execute() {
    updateAlarm();
    if(m_argCnt != 3 || !isNumber(m_args[1], false))
    {
        printLineError("chmod: invalid arguments", false);
        return;
    }
    mode_t mode = stoi(m_args[1], nullptr, 8);
    if(chmod(m_args[2], mode) == -1)
    {

        perror("smash error: chmod failed");
    }
}

SetcoreCommand::SetcoreCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void SetcoreCommand::execute()
{
    updateAlarm();
    if(m_argCnt != 3 || !isNumber(m_args[1], false) || !isNumber(m_args[2], false))
    {
        printLineError("setcore: invalid arguments", false);
        return;
    }

    int jobId = stoi(m_args[1]);
    int core = stoi(m_args[2]);
    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* job = smash.m_jobsList->getJobById(jobId);
    if(job == nullptr)
    {
        std::string msg("smash error: job-id ");
        msg.append(m_args[1]);
        msg.append(" does not exist");
        
        printErrorMessage(msg.c_str());
        return;
    }
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    if (sched_setaffinity(job->getPid(), sizeof(cpuset), &cpuset) == -1)
    {
        perror("smash error: setcore failed");
        return;
    }
}

JobsList::JobEntry::~JobEntry() {
}


void printErrorMessage(std::string msg)
{
    std::cerr << msg << std::endl;
}

void removeChar(std::string& str, char c)
{
    std::string tmp("");
    while(true)
    {
        size_t index = str.find(c);
        if(index == std::string::npos)
            break;
        tmp.append(str.substr(0, index));
        str = str.substr(index + 1, str.size());
    }
    tmp.append(str);
    str = tmp;
}

AlarmList::AlarmList() : m_alarmList()
{
}

void AlarmList::addAlarmEntry(std::string command, time_t duration, pid_t pid) {
    AlarmEntry entry(pid, command, duration);
    m_alarmList.emplace_back(entry);
}

void AlarmList::deleteAlarms() {
    for(auto it = m_alarmList.begin(); it != m_alarmList.end(); ++it)
    {
        AlarmEntry entry = *it;
        if(time(nullptr) >= entry.m_finishTime)
        {
            int res = waitpid(entry.m_pid, NULL, WNOHANG);
            if(res == -1 || res == entry.m_pid)
                return;
            cout << "smash: " << entry.m_cmd << " timed out!" << endl;

            if(kill(entry.m_pid, SIGKILL) == -1)
            {
                perror("smash error: kill failed");
                return;
            }
            m_alarmList.erase(it);
            --it;
        }
    }
}

AlarmList::AlarmEntry::AlarmEntry(pid_t pid, std::string cmd, time_t duration) : m_pid(pid), m_cmd(cmd),
                                                                                 m_startTime(time(nullptr)),
                                                                                 m_duration(duration)
{
    m_finishTime = m_startTime + duration;
}

TimeoutCommand::TimeoutCommand(const char *cmd_line) : BuiltInCommand(cmd_line, true) {

}

void TimeoutCommand::execute() {
    updateAlarm();
    if(m_argCnt < 3)
    {
        printErrorMessage("smash error: timeout: invalid arguments");
        return;
    }
    int durationInt;
    time_t duration;
    try {
        durationInt = stoi(m_args[1]);
        if(durationInt <= 0)
        {
            printErrorMessage("smash error: timeout: invalid arguments");
            return;
        }
        duration = static_cast<time_t>(durationInt);
    } catch (...) {
        printErrorMessage("smash error: timeout: invalid arguments");
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.executeCommand(m_cmd_line.c_str(), true, duration);
}




void setAlarm()
{
    SmallShell &smash = SmallShell::getInstance();
    time_t minFinishTime(time(nullptr));
    bool flag = false;
    if(smash.m_alarmInForeground && smash.m_foregroundFinishTime > time(nullptr))
    {
        minFinishTime = smash.m_foregroundFinishTime;
        flag = true;
    }


    for(auto it = smash.m_alarmsList->m_alarmList.begin(); it != smash.m_alarmsList->m_alarmList.end(); ++it)
    {
        AlarmList::AlarmEntry entry = *it;
        if(!flag || entry.m_finishTime <= minFinishTime)
        {
            if(entry.m_finishTime > time(nullptr))
            {
                flag = true;
                minFinishTime = entry.m_finishTime;
            }
        }
    }
    time_t duration = minFinishTime - time(nullptr);
    alarm((int)duration);
}
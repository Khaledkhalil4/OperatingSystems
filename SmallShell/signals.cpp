#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

void ctrlZHandler(int sig_num) {
    std::cout << "smash: got ctrl-Z"<< std::endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t currPid = smash.m_CurrentPid;
    if(currPid == -1)
        return;
    else
    {

        smash.m_jobsList->addJob(smash.m_currentCommand, currPid, true);
        if(kill(currPid, SIGSTOP) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        smash.m_CurrentPid = -1;
        smash.m_currentCommand = "";
        std::cout << "smash: process " << currPid << " was stopped" << std::endl;
    }
}

void ctrlCHandler(int sig_num) {
    std::cout << "smash: got ctrl-C" << std::endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t currPid = smash.m_CurrentPid;
    if(currPid == -1)
        return;
    else
    {

        if(kill(currPid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        smash.m_CurrentPid = -1;
        smash.m_currentCommand = "";
        std::cout << "smash: process " << currPid << " was killed" << std::endl;
    }
}

void alarmHandler(int sig_num) {
    std::cout << "smash: got an alarm" << std::endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t currPid = smash.m_CurrentPid;
    setAlarm();
    smash.m_jobsList->removeFinishedJobs();
    smash.m_alarmsList->deleteAlarms();
    if(currPid == -1 || !smash.m_alarmInForeground)
        return;

    if(time(nullptr) >= smash.m_foregroundFinishTime)
    {
        smash.m_alarmInForeground = false;
        smash.m_CurrentPid = -1;
        std::string command = smash.m_currentCommand;
        smash.m_currentCommand = "";

        int res = waitpid(currPid, NULL, WNOHANG);
        if(res == -1 || res == currPid)
            return;
        cout << "smash: " << command << " timed out!" << endl;

        if(kill(currPid, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
    }
}

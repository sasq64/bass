#pragma once

#include "log.h"

#include <cstdint>
#include <string>

namespace utils {

#ifdef _WIN32
typedef void* HANDLE;
#endif

struct ExecPipe
{
    ExecPipe() {}
    ExecPipe(const std::string& cmd);
    ~ExecPipe();

    ExecPipe(ExecPipe&& other) = default;
    ExecPipe(const ExecPipe& other) = delete;
    ExecPipe& operator=(const ExecPipe& other) = delete;
    ExecPipe& operator=(ExecPipe&& other) noexcept;

    bool hasEnded();
    void Kill();
    int read(uint8_t* target, int size);
    int write(uint8_t* source, int size);
    operator std::string();

#ifdef _WIN32
    HANDLE hPipeRead;
    HANDLE hPipeWrite;
    HANDLE hProcess;
#else
    pid_t pid = -1;
    int outfd;
    int infd;
#endif
};

#ifdef _WIN32
inline ExecPipe::ExecPipe(const std::string& cmd)
{
    SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES)};
    saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child
                                  // process.
    saAttr.lpSecurityDescriptor = NULL;

    auto c = std::string("cmd.exe /C ") + cmd;

    // Create a pipe to get results from child's stdout.
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
        return;

    PROCESS_INFORMATION pi = {0};

    STARTUPINFO si = {sizeof(STARTUPINFO)};
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = NULL;
    si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing. Requires
                              // STARTF_USESHOWWINDOW in dwFlags.

    BOOL fSuccess = CreateProcessA(NULL, (LPSTR)c.c_str(), NULL, NULL, TRUE,
                                   CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (!fSuccess) {
        LOGD("FAILED %d", GetLastError());
        CloseHandle(hPipeWrite);
        CloseHandle(hPipeRead);
        hProcess = 0;
    }
    hProcess = pi.hProcess;
}

inline int ExecPipe::read(uint8_t* target, int size)
{
    DWORD dwRead = 0;
    DWORD dwAvail = 0;
    int total = 0;

    while (size > 0) {
        if (!::PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
            return -2;

        if (!dwAvail) // no data available, return
            break;

        if (dwAvail > size)
            dwAvail = size;

        if (!::ReadFile(hPipeRead, target, dwAvail, &dwRead, NULL) || !dwRead)
            // error, the child process might ended
            return -2;
        sleepms(1);
        size -= dwRead;
        total += dwRead;
        target += dwRead;
    }

    if (total == 0)
        return -1;

    return total;
}

inline int ExecPipe::write(uint8_t* source, int size)
{
    return 0;
}

ExecPipe& ExecPipe::operator=(ExecPipe&& other) noexcept
{
    hPipeRead = other.hPipeRead;
    hPipeWrite = other.hPipeWrite;
    hProcess = other.hProcess;
    other.hProcess = 0;
    return *this;
}

inline bool ExecPipe::hasEnded()
{
    if (hProcess == 0)
        return true;
    if (WaitForSingleObject(hProcess, 50) == WAIT_OBJECT_0) {
        hProcess = 0;
        return true;
    }
    return false;
}

inline ExecPipe::~ExecPipe() {}

inline void ExecPipe::Kill()
{
    LOGD("KILL %d", hProcess);
    if (hProcess != 0) {
        if (TerminateProcess(hProcess, 0) == 0) {
            LOGD("Could not kill process: %d", GetLastError());
        }
        CloseHandle(hProcess);
    }
}

#else

#    include <fcntl.h>
#    include <signal.h>
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>

inline pid_t popen2(const char* command, int* infp, int* outfp)
{
    enum
    {
        READ,
        WRITE
    };
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return -1;

    pid = fork();

    if (pid < 0)
        return pid;
    if (pid == 0) {
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], WRITE);

        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    if (infp == nullptr)
        close(p_stdin[WRITE]);
    else
        *infp = p_stdin[WRITE];

    if (outfp == nullptr)
        close(p_stdout[READ]);
    else
        *outfp = p_stdout[READ];

    return pid;
}

inline ExecPipe::ExecPipe(const std::string& cmd)
{
    pid = popen2(cmd.c_str(), &infd, &outfd);
    fcntl(outfd, F_SETFL, O_NONBLOCK);
}

inline ExecPipe::~ExecPipe()
{
    int result;
    if (pid != -1) {
        LOGD("Waiting");
        waitpid(pid, &result, 0);
        LOGD("RESULT %d", result);
    }
}

inline ExecPipe& ExecPipe::operator=(ExecPipe&& other) noexcept
{
    pid = other.pid;
    outfd = other.outfd;
    infd = other.infd;
    other.pid = -1;
    return *this;
}

inline void ExecPipe::Kill()
{
    if (pid != -1) {
        int result;
        kill(pid, SIGKILL);
        waitpid(pid, &result, 0);
        pid = -1;
    }
}

// 0 = Done, -1 = Still data left, -2 = error
inline int ExecPipe::read(uint8_t* target, int size)
{
    int rc = ::read(outfd, target, size);
    if (rc == -1 && errno != EAGAIN)
        rc = -2;
    return rc;
}

inline int ExecPipe::write(uint8_t* source, int size)
{
    int rc = ::write(outfd, source, size);
    return rc;
}

inline bool ExecPipe::hasEnded()
{
    if (pid == -1)
        return true;
    int rc;
    if (waitpid(pid, &rc, WNOHANG) == pid) {
        LOGD("PID ended %d %d", pid, rc);
        pid = -1;
        return true;
    }
    return false;
}

#endif

inline ExecPipe::operator std::string()
{
    char buf[1024];
    std::string result;
    bool ended = false;
    while (true) {
        int sz = read(reinterpret_cast<uint8_t*>(&buf[0]), sizeof(buf));
        if (sz > 0) {
            result += std::string(buf, 0, sz);
        } else if (sz != -1 || ended)
            return result;
        ended = hasEnded();
        sleepms(100);
    }
    return result;
}

inline int shellExec(const std::string& cmd, const std::string& binDir)
{
#ifdef _WIN32
    auto cmdLine = utils::format("/C %s", cmd);
    SHELLEXECUTEINFO ShExecInfo = {0};
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = NULL;
    ShExecInfo.lpFile = "cmd.exe";

    ShExecInfo.lpParameters = cmdLine.c_str();
    if (binDir != "")
        ShExecInfo.lpDirectory = binDir.c_str();
    ShExecInfo.nShow = SW_HIDE;
    ShExecInfo.hInstApp = NULL;
    ShellExecuteEx(&ShExecInfo);
    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
    return 0;
#else
    return system((binDir + "/" + cmd).c_str());
#endif
}

inline ExecPipe execPipe(const std::string& cmd)
{
    return ExecPipe(cmd);
}

} // namespace utils

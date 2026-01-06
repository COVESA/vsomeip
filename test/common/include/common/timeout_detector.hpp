#pragma once

#include <cstdint>
#include <csignal>
#include <cstring>
#include <cstdlib>

class timeout_detector {
public:
    timeout_detector(uint32_t _timeout = 180) {
        signal(SIGALRM, alarmHandler);
        (void)!alarm(_timeout);
    }

private:
    static void alarmHandler(int signal) {
        if (signal == SIGALRM) {
            // https://stackoverflow.com/questions/16891019/how-to-avoid-using-printf-in-a-signal-handler
            // can hardly do any logging, but it is important to show what the cause of failure is
            // so use a raw write to stdout/stderr (best-effort, write might not succeed...)
            const char* str = "timeout_detector blew up! THE TEST WILL SIGABRT\n";
            (void)!write(STDOUT_FILENO, str, strlen(str));

            std::abort();
        }
    }
};

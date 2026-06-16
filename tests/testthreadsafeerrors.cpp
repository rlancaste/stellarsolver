#include "testthreadsafeerrors.h"
#include "internalextractorsolver.h"
#include "externalextractorsolver.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <QProcess>
#include <QCoreApplication>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task.h>
#endif

// Portable crash handler to intercept expected crashes on the unfixed codebase.
// This captures signals (like SIGSEGV/SIGABRT) and terminates cleanly with exit code 1,
// avoiding system crash reporting GUI dialogs (e.g., macOS Crash Reporter) during runs.
static void crash_handler(int sig) {
    printf("CRASH DETECTED: Caught signal %d\n", sig);
    fflush(stdout);
    _exit(1);
}

// Disables macOS Crash Reporter GUI popup for this process/thread
static void disable_crash_reporting() {
#ifdef __APPLE__
    task_set_exception_ports(
        mach_task_self(),
        EXC_MASK_ALL,
        MACH_PORT_NULL,
        EXCEPTION_DEFAULT,
        THREAD_STATE_NONE
    );
#endif
}

extern "C" {
#include "astrometry/errors.h"
#include "astrometry/kdtree.h"

// Define mocks for functions needed by errors.c/bl.c/kdtree to avoid linking issues
void debug(const char* format, ...) {
    // Silent mock
}
char* strdup_safe(const char* str) {
    return str ? strdup(str) : nullptr;
}
void asprintf_safe(char** strp, const char* format, ...) {
    // Silent mock
}
void fitsbin_chunk_init(fitsbin_chunk_t* chunk) {
    // Silent mock
}
int kdtree_fits_read_chunk(kdtree_fits_t* io, fitsbin_chunk_t* chunk) {
    // Silent mock
    return 0;
}
}

TestThreadSafeErrors::TestThreadSafeErrors()
{
}

// ==========================================
// 1. Thread-Safe Errors Stack Test
// ==========================================
void TestThreadSafeErrors::runConcurrentErrors()
{
    struct ErrorsTestHelper {
        static void run(TestThreadSafeErrors*) {
            for (int i = 0; i < 1000; ++i) {
                errors_push_state();
                
                report_error("test_module", 42, "test_func", "Concurrent log message %d", i);
                report_errno();

                errors_start_logging_to_string();
                report_error("test_module2", 43, "test_func2", "String log %d", i);
                char* logstr = errors_stop_logging_to_string("\n");
                if (logstr) {
                    if (strstr(logstr, "String log") == nullptr) {
                        printf("ERROR: Log string did not contain expected message! logstr=%s\n", logstr);
                        fflush(stdout);
                        free(logstr);
                        exit(1);
                    }
                    free(logstr);
                }
                
                errors_clear_stack();
                
                errors_pop_state();
            }
        }
    };

    printf("Starting concurrent error reporting thread-safety test (30 threads)...\n");
    fflush(stdout);

    int numThreads = 30;
    QList<QFuture<void>> futures;
    for (int i = 0; i < numThreads; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&ErrorsTestHelper::run, this));
#else
        futures.append(QtConcurrent::run(&ErrorsTestHelper::run, this));
#endif
    }
    for (auto& future : futures) {
        future.waitForFinished();
    }

    printf("Errors Stack Test passed successfully!\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// 2. SolverNum Atomic Uniqueness Test
// ==========================================
void TestThreadSafeErrors::runConcurrentSolverNum()
{
    struct SolverNumTestHelper {
        static void run(TestThreadSafeErrors* instance) {
            for (int i = 0; i < 100; ++i) {
                FITSImage::Statistic stats;
                InternalExtractorSolver* internalSolver = new InternalExtractorSolver(SSolver::SOLVE, SSolver::EXTRACTOR_INTERNAL, SSolver::SOLVER_STELLARSOLVER, stats, nullptr);
                ExternalExtractorSolver* externalSolver = new ExternalExtractorSolver(SSolver::SOLVE, SSolver::EXTRACTOR_INTERNAL, SSolver::SOLVER_STELLARSOLVER, stats, nullptr);
                
                QString internalName = internalSolver->m_BaseName;
                QString externalName = externalSolver->m_BaseName;
                
                delete internalSolver;
                delete externalSolver;
                
                instance->seenNamesMutex.lock();
                if (instance->seenNames.contains(internalName) || instance->seenNames.contains(externalName)) {
                    printf("ERROR: Duplicate baseName detected! internalName=%s, externalName=%s\n",
                           internalName.toUtf8().constData(), externalName.toUtf8().constData());
                    fflush(stdout);
                    instance->seenNamesMutex.unlock();
                    exit(1);
                }
                instance->seenNames.insert(internalName);
                instance->seenNames.insert(externalName);
                instance->seenNamesMutex.unlock();
            }
        }
    };

    printf("Starting concurrent solverNum baseName uniqueness test (30 threads)...\n");
    fflush(stdout);

    int numThreads = 30;
    QList<QFuture<void>> futures;
    for (int i = 0; i < numThreads; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&SolverNumTestHelper::run, this));
#else
        futures.append(QtConcurrent::run(&SolverNumTestHelper::run, this));
#endif
    }
    for (auto& future : futures) {
        future.waitForFinished();
    }

    printf("SolverNum Uniqueness Test passed successfully!\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// 3. KD-Tree Reentrant Sorting Test
// ==========================================
void TestThreadSafeErrors::runConcurrentKDTree()
{
    struct KDTreeTestHelper {
        static void run(TestThreadSafeErrors*) {
            for (int k = 0; k < 100; ++k) {
                int N = 500;
                int D = 3;
                double* data = (double*)malloc(N * D * sizeof(double));
                for (int i = 0; i < N * D; ++i) {
                    data[i] = (double)rand() / RAND_MAX;
                }
                kdtree_t* kd = kdtree_build(nullptr, data, N, D, 16, KDTT_DOUBLE, KD_BUILD_SPLIT);
                if (!kd) {
                    printf("ERROR: kdtree_build failed!\n");
                    fflush(stdout);
                    free(data);
                    exit(1);
                }
                kdtree_free(kd);
                free(data);
            }
        }
    };

    printf("Starting concurrent KD-tree sorting test (30 threads)...\n");
    fflush(stdout);

    int numThreads = 30;
    QList<QFuture<void>> futures;
    for (int i = 0; i < numThreads; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&KDTreeTestHelper::run, this));
#else
        futures.append(QtConcurrent::run(&KDTreeTestHelper::run, this));
#endif
    }
    for (auto& future : futures) {
        future.waitForFinished();
    }

    printf("KD-Tree Sorting Test passed successfully!\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Subprocess Spawning Helper
// ==========================================
static bool runTestInSubprocess(QString testArg, QString testName) {
    printf("Running test: %-30s ... ", testName.toUtf8().constData());
    fflush(stdout);
    
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    
    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args;
    args << testArg;
    
    process.start(appPath, args);
    if (!process.waitForFinished(-1)) {
        printf("FAILED (could not start subprocess)\n");
        fflush(stdout);
        return false;
    }
    
    int exitCode = process.exitCode();
    QProcess::ExitStatus exitStatus = process.exitStatus();
    
    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        printf("FAILED\n");
        QByteArray output = process.readAll();
        printf("--- Subprocess Output ---\n%s\n-------------------------\n", output.constData());
        fflush(stdout);
        return false;
    }
    
    printf("PASSED\n");
    QByteArray output = process.readAll();
    if (!output.isEmpty()) {
        printf("--- Subprocess Output ---\n%s\n-------------------------\n", output.constData());
    }
    fflush(stdout);
    return true;
}

int main(int argc, char *argv[])
{
    // Disables system Crash Reporter GUI popups (like macOS ReportCrash)
    disable_crash_reporting();

    // Register crash signal handlers to capture expected failures cleanly on the unfixed codebase
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
#ifdef SIGBUS
    signal(SIGBUS, crash_handler);
#endif

    QApplication app(argc, argv);
    
    QStringList args = QCoreApplication::arguments();
    if (args.contains("--test-errors")) {
        TestThreadSafeErrors test;
        test.runConcurrentErrors();
        return 0;
    } else if (args.contains("--test-solvernum")) {
        TestThreadSafeErrors test;
        test.runConcurrentSolverNum();
        return 0;
    } else if (args.contains("--test-kdtree")) {
        TestThreadSafeErrors test;
        test.runConcurrentKDTree();
        return 0;
    } else {
        printf("Starting concurrent thread-safety test suite...\n");
        fflush(stdout);
        
        bool errorsPassed = runTestInSubprocess("--test-errors", "Thread-Safe Errors Stack");
        bool solverNumPassed = runTestInSubprocess("--test-solvernum", "SolverNum Atomic Uniqueness");
        bool kdtreePassed = runTestInSubprocess("--test-kdtree", "KD-Tree Reentrant Sorting");
        
        printf("\n========================================\n");
        printf("THREAD-SAFETY TEST SUITE SUMMARY:\n");
        printf("1. Thread-Safe Errors Stack:     %s\n", errorsPassed ? "PASSED" : "FAILED");
        printf("2. SolverNum Atomic Uniqueness:  %s\n", solverNumPassed ? "PASSED" : "FAILED");
        printf("3. KD-Tree Reentrant Sorting:    %s\n", kdtreePassed ? "PASSED" : "FAILED");
        printf("========================================\n");
        fflush(stdout);
        
        if (errorsPassed && solverNumPassed && kdtreePassed) {
            printf("All thread-safety tests passed successfully!\n");
            fflush(stdout);
            return 0;
        } else {
            printf("Some thread-safety tests FAILED!\n");
            fflush(stdout);
            return 1;
        }
    }
}

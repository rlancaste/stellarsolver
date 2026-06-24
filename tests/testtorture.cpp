#include "testtorture.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <QProcess>
#include <QCoreApplication>
#include <QThread>
#include <QThreadPool>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task.h>
#endif

static constexpr int EXTRACT_FLOOD_THREADS = 40;
static constexpr int EXTRACT_FLOOD_ITERS = 20;
static constexpr int SOLVE_FLOOD_THREADS = 20;
static constexpr int SOLVE_FLOOD_ITERS = 5;
static constexpr int INTERLEAVE_SOLVE_THREADS = 15;
static constexpr int INTERLEAVE_SOLVE_ITERS = 3;
static constexpr int INTERLEAVE_EXTRACT_THREADS = 15;
static constexpr int INTERLEAVE_EXTRACT_ITERS = 10;
static constexpr int RAPID_DESTROY_THREADS = 30;
static constexpr int RAPID_DESTROY_ITERS = 50;
static constexpr int RAPID_CLEAN_THREADS = 10;
static constexpr int PARALLEL_THREADS_PER_PROFILE = 10;
static constexpr int PARALLEL_ITERS = 3;
static constexpr int DIFF_PARAMS_THREADS = 20;
static constexpr int DIFF_PARAMS_ITERS = 10;
static constexpr int REPEATED_THREADS = 10;
static constexpr int REPEATED_ITERS = 10;

static constexpr double RA_DEC_TOLERANCE = 0.5;
static constexpr double STAR_COUNT_TOLERANCE = 0.30;

static void crash_handler(int sig) {
    printf("CRASH DETECTED: Caught signal %d\n", sig);
    fflush(stdout);
    _exit(1);
}

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

TestTorture::TestTorture()
{
}

bool TestTorture::loadImages()
{
    fileio loader1;
    if (!loader1.loadImage("randomsky.fits")) {
        printf("ERROR: Failed to load randomsky.fits\n");
        fflush(stdout);
        return false;
    }
    m_statsRandom = loader1.getStats();
    m_bufRandom = loader1.getImageBuffer();

    fileio loader2;
    if (!loader2.loadImage("pleiades.jpg")) {
        printf("ERROR: Failed to load pleiades.jpg\n");
        fflush(stdout);
        return false;
    }
    m_statsPleiades = loader2.getStats();
    m_bufPleiades = loader2.getImageBuffer();

    return true;
}

bool TestTorture::establishBaseline()
{
    {
        StellarSolver *solver = makeSolver(m_statsRandom, m_bufRandom);
        if (!solver->solve()) {
            printf("ERROR: Baseline solve failed for randomsky.fits\n");
            fflush(stdout);
            delete solver;
            return false;
        }
        FITSImage::Solution sol = solver->getSolution();
        m_baselineRA = sol.ra;
        m_baselineDec = sol.dec;
        printf("Baseline solve: RA=%.4f Dec=%.4f pixscale=%.4f\n",
               sol.ra, sol.dec, sol.pixscale);
        fflush(stdout);
        delete solver;
    }

    {
        StellarSolver *solver = new StellarSolver(m_statsRandom, m_bufRandom, nullptr);
        solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
        solver->setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
        solver->setLogLevel(LOG_NONE);
        if (!solver->extract(true)) {
            printf("ERROR: Baseline extract failed for randomsky.fits\n");
            fflush(stdout);
            delete solver;
            return false;
        }
        m_baselineRandomStars = solver->getStarList().count();
        printf("Baseline randomsky stars: %d\n", m_baselineRandomStars);
        fflush(stdout);
        delete solver;
    }

    {
        StellarSolver *solver = new StellarSolver(m_statsPleiades, m_bufPleiades, nullptr);
        solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
        solver->setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
        solver->setLogLevel(LOG_NONE);
        if (!solver->extract(true)) {
            printf("ERROR: Baseline extract failed for pleiades.jpg\n");
            fflush(stdout);
            delete solver;
            return false;
        }
        m_baselinePleiadesStars = solver->getStarList().count();
        printf("Baseline pleiades stars: %d\n", m_baselinePleiadesStars);
        fflush(stdout);
        delete solver;
    }

    return true;
}

StellarSolver *TestTorture::makeSolver(const FITSImage::Statistic &stats,
                                       const uint8_t *buf,
                                       SSolver::Parameters::ParametersProfile profile)
{
    StellarSolver *solver = new StellarSolver(stats, buf, nullptr);
    solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    solver->setProperty("ProcessType", SSolver::SOLVE);
    solver->setParameterProfile(profile);
    solver->setIndexFolderPaths(QStringList() << "astrometry");
    solver->setLogLevel(LOG_NONE);
    return solver;
}

// ==========================================
// Test 1: Concurrent Extract Flood
// ==========================================
void TestTorture::runConcurrentExtractFlood()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(50);

    printf("Starting concurrent extract flood (%d threads x %d iters)...\n",
           EXTRACT_FLOOD_THREADS, EXTRACT_FLOOD_ITERS);
    fflush(stdout);

    struct Helper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < EXTRACT_FLOOD_ITERS; ++i) {
                bool useRandom = (threadId + i) % 2 == 0;
                const FITSImage::Statistic &stats = useRandom ? t->m_statsRandom : t->m_statsPleiades;
                const uint8_t *buf = useRandom ? t->m_bufRandom : t->m_bufPleiades;
                int baseline = useRandom ? t->m_baselineRandomStars : t->m_baselinePleiadesStars;

                StellarSolver *solver = new StellarSolver(stats, buf, nullptr);
                solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
                solver->setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
                solver->setLogLevel(LOG_NONE);

                if (!solver->extract(true)) {
                    printf("ERROR: Thread %d iter %d: extract() returned false\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }

                int starCount = solver->getStarList().count();
                if (starCount == 0) {
                    printf("ERROR: Thread %d iter %d: zero stars extracted\n", threadId, i);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                } else if (baseline > 0) {
                    double ratio = (double)starCount / baseline;
                    if (ratio < (1.0 - STAR_COUNT_TOLERANCE) || ratio > (1.0 + STAR_COUNT_TOLERANCE)) {
                        printf("WARNING: Thread %d iter %d: star count %d vs baseline %d (ratio %.2f)\n",
                               threadId, i, starCount, baseline, ratio);
                        fflush(stdout);
                    }
                }

                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < EXTRACT_FLOOD_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#else
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Extract flood: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Concurrent extract flood passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 2: Concurrent Solve Flood
// ==========================================
void TestTorture::runConcurrentSolveFlood()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(30);

    printf("Starting concurrent solve flood (%d threads x %d iters)...\n",
           SOLVE_FLOOD_THREADS, SOLVE_FLOOD_ITERS);
    fflush(stdout);

    struct Helper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < SOLVE_FLOOD_ITERS; ++i) {
                StellarSolver *solver = t->makeSolver(t->m_statsRandom, t->m_bufRandom);
                if (!solver->solve()) {
                    printf("ERROR: Thread %d iter %d: solve() failed\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }

                FITSImage::Solution sol = solver->getSolution();
                double raDiff = fabs(sol.ra - t->m_baselineRA);
                double decDiff = fabs(sol.dec - t->m_baselineDec);
                if (raDiff > RA_DEC_TOLERANCE || decDiff > RA_DEC_TOLERANCE) {
                    printf("ERROR: Thread %d iter %d: solution RA=%.4f Dec=%.4f deviates from baseline RA=%.4f Dec=%.4f\n",
                           threadId, i, sol.ra, sol.dec, t->m_baselineRA, t->m_baselineDec);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                }

                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < SOLVE_FLOOD_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#else
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Solve flood: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Concurrent solve flood passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 3: Interleaved Solve + Extract
// ==========================================
void TestTorture::runInterleavedSolveExtract()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(40);

    printf("Starting interleaved solve+extract (%d+%d threads)...\n",
           INTERLEAVE_SOLVE_THREADS, INTERLEAVE_EXTRACT_THREADS);
    fflush(stdout);

    struct SolveHelper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < INTERLEAVE_SOLVE_ITERS; ++i) {
                StellarSolver *solver = t->makeSolver(t->m_statsRandom, t->m_bufRandom);
                if (!solver->solve()) {
                    printf("ERROR: Solve thread %d iter %d: solve() failed\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }

                FITSImage::Solution sol = solver->getSolution();
                double raDiff = fabs(sol.ra - t->m_baselineRA);
                double decDiff = fabs(sol.dec - t->m_baselineDec);
                if (raDiff > RA_DEC_TOLERANCE || decDiff > RA_DEC_TOLERANCE) {
                    printf("ERROR: Solve thread %d iter %d: RA=%.4f Dec=%.4f deviates\n",
                           threadId, i, sol.ra, sol.dec);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                }
                delete solver;
            }
        }
    };

    struct ExtractHelper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < INTERLEAVE_EXTRACT_ITERS; ++i) {
                StellarSolver *solver = new StellarSolver(t->m_statsPleiades, t->m_bufPleiades, nullptr);
                solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
                solver->setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
                solver->setLogLevel(LOG_NONE);

                if (!solver->extract(true)) {
                    printf("ERROR: Extract thread %d iter %d: extract() failed\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }

                int count = solver->getStarList().count();
                if (count == 0) {
                    printf("ERROR: Extract thread %d iter %d: zero stars\n", threadId, i);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                }
                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < INTERLEAVE_SOLVE_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&SolveHelper::run, this, i));
#else
        futures.append(QtConcurrent::run(&SolveHelper::run, this, i));
#endif
    }
    for (int i = 0; i < INTERLEAVE_EXTRACT_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&ExtractHelper::run, this, i));
#else
        futures.append(QtConcurrent::run(&ExtractHelper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Interleaved solve+extract: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Interleaved solve+extract passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 4: Rapid Create-Destroy
// ==========================================
void TestTorture::runRapidCreateDestroy()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(50);

    printf("Starting rapid create-destroy (%d destroy threads + %d clean solve threads)...\n",
           RAPID_DESTROY_THREADS, RAPID_CLEAN_THREADS);
    fflush(stdout);

    struct DestroyHelper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < RAPID_DESTROY_ITERS; ++i) {
                StellarSolver *solver = new StellarSolver(t->m_statsRandom, t->m_bufRandom, nullptr);
                solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
                solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
                solver->setProperty("ProcessType", SSolver::SOLVE);
                solver->setParameterProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
                solver->setIndexFolderPaths(QStringList() << "astrometry");
                solver->setLogLevel(LOG_NONE);

                solver->start();
                QThread::msleep(1);
                solver->abortAndWait();
                delete solver;
            }
        }
    };

    struct CleanHelper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < 3; ++i) {
                StellarSolver *solver = t->makeSolver(t->m_statsRandom, t->m_bufRandom);
                if (solver->solve()) {
                    FITSImage::Solution sol = solver->getSolution();
                    double raDiff = fabs(sol.ra - t->m_baselineRA);
                    double decDiff = fabs(sol.dec - t->m_baselineDec);
                    if (raDiff > RA_DEC_TOLERANCE || decDiff > RA_DEC_TOLERANCE) {
                        printf("ERROR: Clean thread %d iter %d: RA=%.4f Dec=%.4f deviates\n",
                               threadId, i, sol.ra, sol.dec);
                        fflush(stdout);
                        t->m_failCount.fetchAndAddRelaxed(1);
                    }
                }
                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < RAPID_DESTROY_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&DestroyHelper::run, this, i));
#else
        futures.append(QtConcurrent::run(&DestroyHelper::run, this, i));
#endif
    }
    for (int i = 0; i < RAPID_CLEAN_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&CleanHelper::run, this, i));
#else
        futures.append(QtConcurrent::run(&CleanHelper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Rapid create-destroy: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Rapid create-destroy passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 5: Parallel Profile Stress
// ==========================================
void TestTorture::runParallelProfileStress()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(30);

    printf("Starting parallel profile stress (%d threads per profile x %d iters)...\n",
           PARALLEL_THREADS_PER_PROFILE, PARALLEL_ITERS);
    fflush(stdout);

    QAtomicInt successCount;

    struct Helper {
        static void run(TestTorture *t, int threadId,
                        SSolver::Parameters::ParametersProfile profile,
                        QAtomicInt *successes) {
            for (int i = 0; i < PARALLEL_ITERS; ++i) {
                StellarSolver *solver = t->makeSolver(t->m_statsRandom, t->m_bufRandom, profile);
                if (solver->solve()) {
                    FITSImage::Solution sol = solver->getSolution();
                    double raDiff = fabs(sol.ra - t->m_baselineRA);
                    double decDiff = fabs(sol.dec - t->m_baselineDec);
                    if (raDiff <= RA_DEC_TOLERANCE && decDiff <= RA_DEC_TOLERANCE) {
                        successes->fetchAndAddRelaxed(1);
                    } else {
                        printf("WARNING: Thread %d iter %d: RA=%.4f Dec=%.4f outside tolerance\n",
                               threadId, i, sol.ra, sol.dec);
                        fflush(stdout);
                    }
                }
                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < PARALLEL_THREADS_PER_PROFILE; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&Helper::run, this, i,
                       SSolver::Parameters::PARALLEL_SMALLSCALE, &successCount));
        futures.append(QtConcurrent::run(&Helper::run, this, i + PARALLEL_THREADS_PER_PROFILE,
                       SSolver::Parameters::PARALLEL_LARGESCALE, &successCount));
#else
        futures.append(QtConcurrent::run(&Helper::run, this, i,
                       SSolver::Parameters::PARALLEL_SMALLSCALE, &successCount));
        futures.append(QtConcurrent::run(&Helper::run, this, i + PARALLEL_THREADS_PER_PROFILE,
                       SSolver::Parameters::PARALLEL_LARGESCALE, &successCount));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int totalAttempts = PARALLEL_THREADS_PER_PROFILE * 2 * PARALLEL_ITERS;
    int successes = successCount.loadRelaxed();
    printf("Parallel profile stress: %d/%d successful solves\n", successes, totalAttempts);
    fflush(stdout);

    if (successes < totalAttempts / 2) {
        printf("ERROR: Less than 50%% success rate\n");
        fflush(stdout);
        exit(1);
    }

    printf("Parallel profile stress passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 6: Extract with Different Params
// ==========================================
void TestTorture::runExtractDifferentParams()
{
    if (!loadImages()) exit(1);
    if (!establishBaseline()) exit(1);

    QThreadPool::globalInstance()->setMaxThreadCount(30);

    printf("Starting extract with different params (%d threads x %d iters)...\n",
           DIFF_PARAMS_THREADS, DIFF_PARAMS_ITERS);
    fflush(stdout);

    struct Helper {
        static void run(TestTorture *t, int threadId) {
            SSolver::Parameters::ParametersProfile profiles[] = {
                SSolver::Parameters::DEFAULT,
                SSolver::Parameters::SINGLE_THREAD_SOLVING,
                SSolver::Parameters::ALL_STARS,
                SSolver::Parameters::SMALL_STARS,
            };
            int numProfiles = sizeof(profiles) / sizeof(profiles[0]);

            for (int i = 0; i < DIFF_PARAMS_ITERS; ++i) {
                SSolver::Parameters::ParametersProfile profile = profiles[(threadId + i) % numProfiles];
                bool withHFR = (threadId + i) % 2 == 0;

                StellarSolver *solver = new StellarSolver(t->m_statsPleiades, t->m_bufPleiades, nullptr);
                solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
                solver->setProperty("ProcessType", SSolver::EXTRACT_WITH_HFR);
                solver->setParameterProfile(profile);
                solver->setLogLevel(LOG_NONE);

                if (!solver->extract(withHFR)) {
                    printf("ERROR: Thread %d iter %d: extract failed\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }

                int count = solver->getStarList().count();
                if (count == 0) {
                    printf("ERROR: Thread %d iter %d: zero stars\n", threadId, i);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                }
                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < DIFF_PARAMS_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#else
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Extract different params: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Extract with different params passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Test 7: Repeated Load-Solve Lifecycle
// ==========================================
void TestTorture::runRepeatedLoadSolve()
{
    printf("Starting repeated load-solve (%d threads x %d iters)...\n",
           REPEATED_THREADS, REPEATED_ITERS);
    fflush(stdout);

    QThreadPool::globalInstance()->setMaxThreadCount(20);

    struct Helper {
        static void run(TestTorture *t, int threadId) {
            for (int i = 0; i < REPEATED_ITERS; ++i) {
                fileio loader;
                if (!loader.loadImage("randomsky.fits")) {
                    printf("ERROR: Thread %d iter %d: failed to load image\n", threadId, i);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                    continue;
                }
                FITSImage::Statistic stats = loader.getStats();
                uint8_t *buf = loader.getImageBuffer();

                StellarSolver *solver = new StellarSolver();
                solver->loadNewImageBuffer(stats, buf);
                solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
                solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
                solver->setProperty("ProcessType", SSolver::SOLVE);
                solver->setParameterProfile(SSolver::Parameters::SINGLE_THREAD_SOLVING);
                solver->setIndexFolderPaths(QStringList() << "astrometry");
                solver->setLogLevel(LOG_NONE);

                if (!solver->solve()) {
                    printf("WARNING: Thread %d iter %d: solve failed\n", threadId, i);
                    fflush(stdout);
                    delete solver;
                    continue;
                }

                FITSImage::Solution sol = solver->getSolution();
                if (sol.ra < 1.0 && sol.dec < 1.0) {
                    printf("ERROR: Thread %d iter %d: suspicious solution RA=%.4f Dec=%.4f\n",
                           threadId, i, sol.ra, sol.dec);
                    fflush(stdout);
                    t->m_failCount.fetchAndAddRelaxed(1);
                }

                delete solver;
            }
        }
    };

    QList<QFuture<void>> futures;
    for (int i = 0; i < REPEATED_THREADS; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#else
        futures.append(QtConcurrent::run(&Helper::run, this, i));
#endif
    }
    for (auto &f : futures) f.waitForFinished();

    int fails = m_failCount.loadRelaxed();
    if (fails > 0) {
        printf("Repeated load-solve: %d failures detected\n", fails);
        fflush(stdout);
        exit(1);
    }

    printf("Repeated load-solve passed\n");
    fflush(stdout);
    exit(0);
}

// ==========================================
// Subprocess Spawning Helper
// ==========================================
static bool runTestInSubprocess(QString testArg, QString testName, int timeoutMs) {
    printf("Running test: %-35s ... ", testName.toUtf8().constData());
    fflush(stdout);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args;
    args << testArg;

    process.start(appPath, args);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(5000);
        printf("FAILED (timeout after %ds)\n", timeoutMs / 1000);
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
    disable_crash_reporting();

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
#ifdef SIGBUS
    signal(SIGBUS, crash_handler);
#endif

    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif

    QStringList args = QCoreApplication::arguments();

    if (args.contains("--test-concurrent-extract-flood")) {
        TestTorture test;
        test.runConcurrentExtractFlood();
        return 0;
    } else if (args.contains("--test-concurrent-solve-flood")) {
        TestTorture test;
        test.runConcurrentSolveFlood();
        return 0;
    } else if (args.contains("--test-interleaved-solve-extract")) {
        TestTorture test;
        test.runInterleavedSolveExtract();
        return 0;
    } else if (args.contains("--test-rapid-create-destroy")) {
        TestTorture test;
        test.runRapidCreateDestroy();
        return 0;
    } else if (args.contains("--test-parallel-profile-stress")) {
        TestTorture test;
        test.runParallelProfileStress();
        return 0;
    } else if (args.contains("--test-extract-different-params")) {
        TestTorture test;
        test.runExtractDifferentParams();
        return 0;
    } else if (args.contains("--test-repeated-load-solve")) {
        TestTorture test;
        test.runRepeatedLoadSolve();
        return 0;
    } else {
        printf("=== StellarSolver Thread-Safety Torture Test Suite ===\n\n");
        fflush(stdout);

        bool results[7];
        const char *names[] = {
            "Concurrent Extract Flood",
            "Concurrent Solve Flood",
            "Interleaved Solve+Extract",
            "Rapid Create-Destroy",
            "Parallel Profile Stress",
            "Extract Different Params",
            "Repeated Load-Solve",
        };
        const char *flags[] = {
            "--test-concurrent-extract-flood",
            "--test-concurrent-solve-flood",
            "--test-interleaved-solve-extract",
            "--test-rapid-create-destroy",
            "--test-parallel-profile-stress",
            "--test-extract-different-params",
            "--test-repeated-load-solve",
        };
        int timeouts[] = {
            120000,
            180000,
            180000,
            90000,
            300000,
            120000,
            180000,
        };

        for (int i = 0; i < 7; ++i) {
            results[i] = runTestInSubprocess(flags[i], names[i], timeouts[i]);
        }

        printf("\n========================================\n");
        printf("TORTURE TEST SUITE SUMMARY:\n");
        int passed = 0;
        for (int i = 0; i < 7; ++i) {
            printf("%d. %-35s %s\n", i + 1, names[i], results[i] ? "PASSED" : "FAILED");
            if (results[i]) passed++;
        }
        printf("========================================\n");
        printf("%d/7 tests passed\n", passed);
        fflush(stdout);

        return (passed == 7) ? 0 : 1;
    }
}

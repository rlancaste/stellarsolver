#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef stellarsolver_EXPORTS
#define STELLARSOLVER_API __declspec(dllexport)
#else
#define STELLARSOLVER_API __declspec(dllimport)
#endif
#else
#define STELLARSOLVER_API __attribute__((visibility("default")))
#endif

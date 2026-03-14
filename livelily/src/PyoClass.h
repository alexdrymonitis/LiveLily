#ifndef PYOCLASS_H
#define PYOCLASS_H

#ifdef USEPYO
#include "m_pyo.h"
#endif
#include <string>
#include <vector>

#define PYOMSGSIZE 262144

#ifdef USEPYO
typedef int callPtr(int);

class Pyo {
    public:
        ~Pyo();
        void setup(int inChannels, int outChannels, int bufferSize, int sampleRate);
		std::vector<std::string> getStdout();
        void process(float *buffer);
        void fillin(float *buffer);
        void clear();
        int loadfile(const char *file, int add);
        int exec(const char *msg, int debug);
		std::string getErrorMsg();
        int value(const char *name, float value);
        int value(const char *name, float *value, int len);
        int set(const char *name, float value);
        int set(const char *name, float *value, int len);

    private:
        int inChannels;
        int outChannels;
        int bufferSize;
        int sampleRate;
		int debug;
        PyThreadState *interpreter;
        float *pyoInBuffer;
        float *pyoOutBuffer;
        callPtr *pyoCallback;
        int pyoId;
        char pyoMsg[PYOMSGSIZE];
};
//#else
//class Pyo {
//	public:
//		~Pyo();
//        void setup(int inChannels, int outChannels, int bufferSize, int sampleRate) {};
//};
#endif

#endif

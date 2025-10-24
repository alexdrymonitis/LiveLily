#include "PyoClass.h"

#ifdef USEPYO
/*
** Creates a python interpreter and initialize a pyo server inside it.
** This function must be called, once per Pyo object, before any other
** calls.
**
** arguments:
**   nChannels : int, number of in/out channels.
**   bufferSize : int, number of samples per buffer.
**   sampleRate : int, sample rate frequency.
**
** All arguments should be equal to the host audio settings.
*/
void Pyo::setup(int _nChannels, int _bufferSize, int _sampleRate) {
    nChannels = _nChannels;
    bufferSize = _bufferSize;
    sampleRate = _sampleRate;
    interpreter = pyo_new_interpreter(sampleRate, bufferSize, nChannels);
    pyoInBuffer = reinterpret_cast<float*>(pyo_get_input_buffer_address(interpreter));
    pyoOutBuffer = reinterpret_cast<float*>(pyo_get_output_buffer_address(interpreter));
    pyoCallback = reinterpret_cast<callPtr*>(pyo_get_embedded_callback_address(interpreter));
    pyoId = pyo_get_server_id(interpreter);
}

/*
** Gets updated from ecitor.cpp
** All it does is check if there is a message in the queue by Python's stdout
** and returns it to the editor so the message can be stored in LiveLily's traceback 
*/
std::vector<std::string> Pyo::getStdout() {
    std::vector<std::string> out;
    char *msg;
    while (pyo_dequeue_stdout(&msg)) {
        out.emplace_back(msg);   // copy into vector
        free(msg);               // free allocated C buffer
    }
    return out;
}

/*
** Terminates this object's interpreter.
*/
Pyo::~Pyo() {
    pyo_end_interpreter(interpreter);
}

/*
** This function fills pyo's input buffers with new samples. Should be called
** once per process block, inside the host's audioIn function.
**
** arguments:
**   *buffer : float *, float pointer pointing to the host's input buffers.
*/
void Pyo::fillin(float *buffer) {
    for (int i=0; i<(bufferSize*nChannels); i++) pyoInBuffer[i] = buffer[i];
}


/*
** This function tells pyo to process a buffer of samples and fills the host's
** output buffer with new samples. Should be called once per process block,
** inside the host's audioOut function.
**
** arguments:
**   *buffer : float *, float pointer pointing to the host's output buffers.
*/
void Pyo::process(float *buffer) {
    pyoCallback(pyoId);
    for (int i=0; i<(bufferSize*nChannels); i++) buffer[i] = pyoOutBuffer[i];
}

/*
** Execute a python script "file" in the objectès thread's interpreter.
** An integer "add" is needed to indicate if the pyo server should be
** reboot or not.
**
** arguments:
**   file : char *, filename to execute as a python script. The file is first
**                  searched in the current working directory. If not found,
**                  the module will try to open it as an absolute path.
**   add, int, if positive, the commands in the file will be added to whatever
**             is already running in the pyo server. If 0, the server will be
**             cleared before executing the file.
**
** returns 0 (no error), 1 (failed to open the file) or 2 (bad code in file).
*/
int Pyo::loadfile(const char *file, int add) {
    return pyo_exec_file(interpreter, file, pyoMsg, add);
}

/*
** Sends a numerical value to an existing Sig or SigTo object.
**
** arguments:
**   name : const char *, variable name of the object.
**   value : float, value to be assigned.
**
** returns 0 (no error) or 1 (bad code in file).
**
** Example:
**
** inside the script file:
**
** freq = SigTo(value=440, time=0.1, init=440)
**
** Inside OpenFrameworks (for a Pyo object named `pyo`):
**
** pyo.value("freq", 880);
*/
int Pyo::value(const char *name, float value) {
    sprintf(pyoMsg, "%s.value=%f", name, value);
    return pyo_exec_statement(interpreter, pyoMsg, 0);
}

/*
** Sends an array of numerical values to an existing Sig or SigTo object.
**
** arguments:
**   name : const char *, variable name of the object.
**   value : float *, array of floats.
**   len : int, number of elements in the array.
**
** returns 0 (no error) or 1 (bad code in file).
**
** Example:
**
** inside the script file:
**
** freq = SigTo(value=[100,200,300,400], time=0.1, init=[100,200,300,400])
**
** Inside OpenFrameworks (for a Pyo object named `pyo`):
**
** float frequencies[4] = {150, 250, 350, 450};
** pyo.value("freq", frequencies, 4);
*/
int Pyo::value(const char *name, float *value, int len) {
    char fchar[32];
    sprintf(pyoMsg, "%s.value=[", name);
    for (int i=0; i<len; i++) {
        sprintf(fchar, "%f,", value[i]);
        strcat(pyoMsg, fchar);
    }
    strcat(pyoMsg, "]");
    return pyo_exec_statement(interpreter, pyoMsg, 0);
}

/*
** Sends a numerical value to a Pyo object's attribute.
**
** arguments:
**   name : const char *, object name and attribute separated by a dot.
**   value : float, value to be assigned.
**
** returns 0 (no error) or 1 (bad code in file).
**
** Example:
**
** inside the script file:
**
** filter = Biquad(input=Noise(0.5), freq=1000, q=4, type=2)
**
** Inside OpenFrameworks (for a Pyo object named `pyo`):
**
** pyo.set("filter.freq", 2000);
*/
int Pyo::set(const char *name, float value) {
    sprintf(pyoMsg, "%s=%f", name, value);
    return pyo_exec_statement(interpreter, pyoMsg, 0);
}

/*
** Sends an array of numerical values to a Pyo object's attribute.
**
** arguments:
**   name : const char *, object name and attribute separated by a dot.
**   value : float *, array of floats.
**   len : int, number of elements in the array.
**
** returns 0 (no error) or 1 (bad code in file).
**
** Example:
**
** inside the script file:
**
** filters = Biquad(input=Noise(0.5), freq=[250, 500, 1000, 2000], q=5, type=2)
**
** Inside OpenFrameworks (for a Pyo object named `pyo`):
**
** float frequencies[4] = {350, 700, 1400, 2800};
** pyo.set("filters.freq", frequencies, 4);
*/
int Pyo::set(const char *name, float *value, int len) {
    char fchar[32];
    sprintf(pyoMsg, "%s=[", name);
    for (int i=0; i<len; i++) {
        sprintf(fchar, "%f,", value[i]);
        strcat(pyoMsg, fchar);
    }
    strcat(pyoMsg, "]");
    return pyo_exec_statement(interpreter, pyoMsg, 0);
}

/*
** Executes any raw valid python statement. With this function, one can dynamically
** creates and manipulates audio objects and algorithms.
**
** arguments:
**   msg : const char *, pointer to a string containing the statement to execute.
**
** returns 0 (no error) or 1 (bad code in file).
**
** Example (for a Pyo object named `pyo`):
**
** pyo.exec("pits = [0.001, 0.002, 0.003, 0.004]")
** pyo.exec("fr = Rossler(pitch=pits, chaos=0.9, mul=250, add=500)")
** pyo.exec("b = SumOsc(freq=fr, ratio=0.499, index=0.4, mul=0.2).out()")
*/
int Pyo::exec(const char *_msg, int debug) {
    strcpy(pyoMsg, _msg);
    return pyo_exec_statement(interpreter, pyoMsg, debug);
}

/*
** Return the error message stored to the pyoMsg char array
** in case the function above returns a non-zero error code
*/
std::string Pyo::getErrorMsg() {
	std::string s(pyoMsg);
	return s;
}

/*
** Shutdown and reboot the pyo server while keeping current in/out buffers.
** This will erase audio objects currently active within the server.
**
*/void Pyo::clear() {
    pyo_server_reboot(interpreter);
}
#endif

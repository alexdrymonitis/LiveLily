#include "PyoClass.h"
#include <iostream>

static std::string evalString(PyThreadState *interp, bool& helpersDefined, const std::string& expr);

//Pyo::Pyo() {
//	gstate = PyGILState_Ensure();  // Acquire GIL
//}

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
void Pyo::setup(int _inChannels, int _outChannels, int _bufferSize, int _sampleRate) {
	inChannels = _inChannels;
	outChannels = _outChannels;
	bufferSize = _bufferSize;
	sampleRate = _sampleRate;
	debug = 0;
	interpreter = pyo_new_interpreter(sampleRate, bufferSize, inChannels, outChannels);
	pyoInBuffer = reinterpret_cast<float*>(pyo_get_input_buffer_address(interpreter));
	pyoOutBuffer = reinterpret_cast<float*>(pyo_get_output_buffer_address(interpreter));
	pyoCallback = reinterpret_cast<callPtr*>(pyo_get_embedded_callback_address(interpreter));
	pyoId = reinterpret_cast<void *>(pyo_get_server_address(interpreter));
	processing = false;
	// the helper functions replace help(), so they are defined before any code is executed (see autocompletionHelpers)
	evalString(interpreter, helpersDefined, "''");
}

/*
** Terminates this object's interpreter.
*/
Pyo::~Pyo() {
	// the interpreter exists only if setup() has been called
	if (interpreter) pyo_end_interpreter(interpreter);
}

/*
** This function fills pyo's input buffers with new samples. Should be called
** once per process block, inside the host's audioIn function.
**
** arguments:
**   *buffer : float *, float pointer pointing to the host's input buffers.
*/
void Pyo::fillin(float *buffer) {
	processing = true;
	for (int i=0; i<(bufferSize*inChannels); i++) pyoInBuffer[i] = buffer[i];
	processing = false;
}

bool Pyo::isProcessing()
{
	return processing;
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
	processing = true;
	for (int i=0; i<(bufferSize*outChannels); i++) buffer[i] = pyoOutBuffer[i];
	processing = false;
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
	int err = pyo_exec_file(interpreter, file, pyoMsg, add, debug);
	executions++;
	return err;
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
	return pyo_exec_statement(interpreter, pyoMsg, debug);
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
	return pyo_exec_statement(interpreter, pyoMsg, debug);
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
	return pyo_exec_statement(interpreter, pyoMsg, debug);
}

/*
** Executes any raw valid python statement. With this function, one can dynamically
** create and manipulate audio objects and algorithms.
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
int Pyo::exec(const char *_msg) {
	strcpy(pyoMsg, _msg);
	int err = pyo_exec_statement(interpreter, pyoMsg, debug);
	executions++;
	return err;
}

/*
** Executes any raw valid python statement in the main thread. With this function, one can dynamically
** create and manipulate audio objects and algorithms.
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
int Pyo::exec_main(const char *_msg) {
	strcpy(pyoMsg, _msg);
	int err = pyo_exec_statement_main(pyoMsg, debug);
	executions++;
	return err;
}

/*
** The helper functions of autocompletion, which are defined in the interpreter the first time they are called
** an object is looked up with inspect.getattr_static(), so that no code runs (e.g. the getter of a property)
*/
static const char *autocompletionHelpers =
	"def _livelily_object_(dotted):\n"
	"    import builtins, inspect\n"
	"    parts = dotted.split('.')\n"
	"    g = globals()\n"
	"    obj = g[parts[0]] if parts[0] in g else getattr(builtins, parts[0])\n"
	"    for part in parts[1:]:\n"
	"        obj = inspect.getattr_static(obj, part)\n"
	"        # a property or another descriptor gives its value only by running code, so its value is not known\n"
	"        if hasattr(type(obj), '__get__') and not (inspect.isroutine(obj) or inspect.isclass(obj) or inspect.ismodule(obj)):\n"
	"            raise AttributeError(part)\n"
	"    return obj\n"
	"def _livelily_globals_():\n"
	"    import builtins\n"
	"    names = list(globals().keys()) + dir(builtins)\n"
	"    return '\\n'.join(sorted(set(n for n in names if isinstance(n, str) and n.isidentifier())))\n"
	"def _livelily_names_(dotted):\n"
	"    try:\n"
	"        obj = _livelily_object_(dotted)\n"
	"        # a class can make dir() list only some of its names (pyo objects list only their attributes, e.g. freq and mul)\n"
	"        # so the names Python lists by default, with the methods, are added\n"
	"        names = set(dir(obj)) | set(object.__dir__(obj))\n"
	"        return '\\n'.join(sorted(set(n for n in names if isinstance(n, str) and n.isidentifier())))\n"
	"    except Exception:\n"
	"        return ''\n"
	"def _livelily_signature_(dotted):\n"
	"    import inspect\n"
	"    try:\n"
	"        params = inspect.signature(_livelily_object_(dotted)).parameters.values()\n"
	"    except Exception:\n"
	"        return ''\n"
	"    out = []\n"
	"    for p in params:\n"
	"        if p.name == 'self' or p.kind in (p.VAR_POSITIONAL, p.VAR_KEYWORD):\n"
	"            continue\n"
	"        if p.default is p.empty:\n"
	"            out.append(p.name)\n"
	"            continue\n"
	"        default = repr(p.default)\n"
	"        # a default that can't be written as code (e.g. <pyo.Sig object at 0x...>) or a very long one is left out\n"
	"        out.append(p.name if (default.startswith('<') or '\\n' in default or len(default) > 40) else p.name + '=' + default)\n"
	"    return '\\n'.join(out)\n"
	"# help(name) opens the help of name in the pane that executes it, as \\help in LiveLily (see Editor::loadHelpText())\n"
	"# so it stores the help text, which the pane takes after executing its code (see Pyo::takeHelp())\n"
	"_livelily_help_ = None\n"
	"def _livelily_open_help_(*args, **kwargs):\n"
	"    global _livelily_help_\n"
	"    import inspect, pydoc\n"
	"    if not args:\n"
	"        print('help() takes the object or the name of its help, e.g. help(Sine)')\n"
	"        return\n"
	"    obj = args[0]\n"
	"    # the help of an object (e.g. a of a = Sine()) is the help of its class\n"
	"    if not (isinstance(obj, str) or inspect.ismodule(obj) or inspect.isclass(obj) or inspect.isroutine(obj)):\n"
	"        obj = type(obj)\n"
	"    try:\n"
	"        text = pydoc.render_doc(obj, title='%s', renderer=pydoc.plaintext)\n"
	"    except Exception as e:\n"
	"        # only the first line of the error of pydoc, without its advice about the interactive help, which help() is not here\n"
	"        print(str(e).split('\\n')[0])\n"
	"        return\n"
	"    name = obj if isinstance(obj, str) else getattr(obj, '__name__', type(obj).__name__)\n"
	"    _livelily_help_ = name + '\\n' + text\n"
	"def _livelily_take_help_():\n"
	"    global _livelily_help_\n"
	"    text = _livelily_help_ or ''\n"
	"    _livelily_help_ = None\n"
	"    return text\n"
	"import builtins\n"
	"builtins.help = _livelily_open_help_\n";

/*
** Evaluates a Python expression that returns a string in the interpreter, and returns this string (empty on error)
*/
static std::string evalString(PyThreadState *interp, bool& helpersDefined, const std::string& expr) {
	std::string result;
	if (!interp) return result;
	PyEval_AcquireThread(interp);
	PyObject *module = PyImport_AddModule("__main__"); // borrowed reference
	PyObject *globals = module ? PyModule_GetDict(module) : NULL; // borrowed reference
	if (globals) {
		if (!helpersDefined) {
			PyObject *defined = PyRun_String(autocompletionHelpers, Py_file_input, globals, globals);
			if (defined) {
				helpersDefined = true;
				Py_DECREF(defined);
			}
			else {
				PyErr_Clear();
			}
		}
		if (helpersDefined) {
			PyObject *obj = PyRun_String(expr.c_str(), Py_eval_input, globals, globals);
			if (obj) {
				if (PyUnicode_Check(obj)) {
					const char *s = PyUnicode_AsUTF8(obj);
					if (s) result = s;
				}
				Py_DECREF(obj);
			}
			else {
				PyErr_Clear();
			}
		}
	}
	PyEval_ReleaseThread(interp);
	return result;
}

/*
** The dotted name is passed as a Python string literal, with only the characters of names and dots
*/
static std::string quotedName(const std::string& dotted) {
	std::string clean;
	for (char c : dotted) {
		if (isalnum((unsigned char)c) || c == '_' || c == '.') clean += c;
	}
	return "'" + clean + "'";
}

std::string Pyo::takeHelp() {
	return evalString(interpreter, helpersDefined, "_livelily_take_help_()");
}

std::string Pyo::globalNames() {
	return evalString(interpreter, helpersDefined, "_livelily_globals_()");
}

std::string Pyo::names(const std::string& dotted) {
	if (dotted.empty()) return "";
	return evalString(interpreter, helpersDefined, "_livelily_names_(" + quotedName(dotted) + ")");
}

std::string Pyo::signature(const std::string& dotted) {
	if (dotted.empty()) return "";
	return evalString(interpreter, helpersDefined, "_livelily_signature_(" + quotedName(dotted) + ")");
}

/*
** Set the debug state
*/
void Pyo::setDebug(int debugVal) {
	debug = debugVal;
}

/*
** Get the STDOUT as a vector of strings
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
** Return the error message stored to the pyoMsg char array
** in case the exec() or loadfile() returns a non-zero error code
*/
std::string Pyo::getErrorMsg() {
	std::string s(pyoMsg);
	return s;
}

/*
** Shutdown and reboot the pyo server while keeping current in/out buffers.
** This will erase audio objects currently active within the server.
**
*/
void Pyo::clear() {
	pyo_server_reboot(interpreter);
}

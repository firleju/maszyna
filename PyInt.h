#ifndef PyIntH
#define PyIntH

#undef _DEBUG // bez tego macra Py_DECREF powoduja problemy przy linkowaniu

#include <vector>
#include <set>
#include <string>

#include "Python.h"
#include "parser.h"
#include "Model3d.h"

#define PyGetFloat(param) PyFloat_FromDouble(param >= 0 ? param : -param)
#define PyGetFloatS(param) PyFloat_FromDouble(param)
#define PyGetInt(param) PyInt_FromLong(param)
#define PyGetFloatS(param) PyFloat_FromDouble(param)
#define PyGetBool(param) param ? Py_True : Py_False
#define PyGetString(param) PyString_FromString(param)

struct ltstr
{
    bool operator()(const char *s1, const char *s2) const
    {
        return strcmp(s1, s2) < 0;
    }
};

class TPythonInterpreter
{
  protected:
    TPythonInterpreter();
	~TPythonInterpreter() {}
    static TPythonInterpreter *_instance;
    int _screenRendererPriority;
//    std::set<const char *, ltstr> _classes;
	std::set<std::string const> _classes;
	PyObject *_main;
    PyObject *_stdErr;
//    FILE *_getFile(const char *lookupPath, const char *className);
	FILE *_getFile( std::string const &lookupPath, std::string const &className );

  public:
    static TPythonInterpreter *getInstance();
	static void killInstance();
/*  bool loadClassFile(const char *lookupPath, const char *className);
    PyObject *newClass(const char *className);
    PyObject *newClass(const char *className, PyObject *argsTuple);
*/	bool loadClassFile( std::string const &lookupPath, std::string const &className );
	PyObject *newClass( std::string const &className );
	PyObject *newClass( std::string const &className, PyObject *argsTuple );
	int getScreenRendererPriotity()
    {
        return _screenRendererPriority;
    };
    void setScreenRendererPriority(const char *priority);
    void handleError();
};

class TPythonScreenRenderer
{
  protected:
    PyObject *_pyRenderer;
    PyObject *_pyTexture;
    int _textureId;
    PyObject *_pyWidth;
    PyObject *_pyHeight;

  public:
    TPythonScreenRenderer(int textureId, PyObject *renderer);
    ~TPythonScreenRenderer();
    void render(PyObject *trainState);
    void cleanup();
    void updateTexture();
};

class TPythonScreens
{
  protected:
    bool _cleanupReadyFlag;
    bool _renderReadyFlag;
    bool _terminationFlag;
    void *_thread;
    unsigned int _threadId;
    std::vector<TPythonScreenRenderer *> _screens;
    std::string _lookupPath;
    void *_train;
    void _cleanup();
    void _freeTrainState();
    PyObject *_trainState;

  public:
    void reset(void *train);
    void setLookupPath(std::string const &path);
    void init(cParser &parser, TModel3d *model, std::string const &name, int const cab);
    void update();
    TPythonScreens();
    ~TPythonScreens();
    void run();
    void start();
    void finish();
};

#endif // PyIntH

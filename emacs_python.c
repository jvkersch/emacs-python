#include <Python.h>
#include <emacs-module.h>


/* Needed by Emacs to ascertain that this module is properly GPL-licensed. */
int plugin_is_GPL_compatible = 1;

/* Holds a string representation of the last Python error. */
#define ERROR_BUF_SIZE 10000
static char error_buf[ERROR_BUF_SIZE];

#define set_error(...) snprintf(error_buf, ERROR_BUF_SIZE, __VA_ARGS__)


/* Signal an error to Emacs and return nil. */
#define EMACS_HANDLE_ERROR(env) do { \
    emacs_value errmsg = env->make_string(env, error_buf, strlen(error_buf)); \
    env->non_local_exit_signal(env, env->intern (env, "error"), errmsg); \
    return env->intern(env, "nil"); \
} while (0);


/* Main Python module (created after the interpreter is started). */
static PyObject *module = NULL;


/* Copy a string out of an emacs argument. Returns a new pointer to a string on
 * the heap. It's the user's responsibility to free this pointer. */
static const char*
copy_string_from_emacs(emacs_env *env, emacs_value arg)
{
    ptrdiff_t s = 1024;
    char *str = (char *)malloc(s);
    if (str == NULL) {
        /* TODO provide more info about what went wrong */
        return NULL;
    }

    for (int i = 0; i < 2; i++) {
        if (env->copy_string_contents(env, arg, str, &s)) {
            return str;
        }

        /* The buffer wasn't big enough to capture the string from
         * emacs. Realloc and try again. */
        str = reallocf(str, s);
        if (str == NULL) {
            /* TODO provide more info about what went wrong */
            return NULL;
        }
    }

    return NULL;
}

/* TODO docstring here */
static void
init_python_environment()
{
    Py_Initialize();

    module = PyImport_AddModule("__main__");
}


/* Get a string representation in "error_buf" of the last Python error that
   occurred. This clears out Python's error indicator. */
static void
py_err()
{
    PyObject *ptype, *pvalue, *ptraceback, *py_err;
    char *msg = NULL;

    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_Clear();
    if (!pvalue)
        return;

    py_err = PyObject_Str(pvalue);
    if ( (!py_err) || !(msg = PyString_AsString(py_err)))
        msg = "<unknown error>";

    /* Copy the string into buffer before discarding pvalue. */
    snprintf(error_buf, ERROR_BUF_SIZE, "Python error: %s", msg);

    Py_XDECREF(py_err);

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
}


/* Call a Python function referenced by name, with a given number of
 * arguments. Returns a *new ref* to the result. */
static PyObject*
call_python_function(const char *name, PyObject* args)
{
    PyObject *v, *f;

    f = PyObject_GetAttrString(module, name);
    if (f == NULL) {
        /* Fall back to builtins dictionary */
        PyObject *builtins = PyEval_GetBuiltins();
        f = PyDict_GetItemString(builtins, name);
    }
    if (f == NULL) {
        PyErr_Format(PyExc_AttributeError, "No such attribute: %s", name);
        goto error;
    }

    v = PyObject_CallObject(f, args);
    if (v == NULL)
        goto error;

    return v;

error:
    py_err();
    return NULL;
}


/* Execute a snippet of Python code in the embedded interpreter. Return value
 * is 0 if the execution completed successfully, or -1 (with an error set) if
 * not. */
static int
exec_python_code(const char *code)
{
    PyObject *locals = PyModule_GetDict(module);
    if (locals == NULL)
        goto error;

    PyObject *v = PyRun_String(code, Py_file_input, locals, locals);
    if (v == NULL)
        goto error;

    Py_DECREF(v);
    return 0;

error:
    py_err();
    return -1;
}


/* Convert a single Emacs symbol to its equivalent Python representation. */
static PyObject*
convert_emacs_symbol(emacs_env *env, emacs_value symbol)
{
    emacs_value typ = env->type_of(env, symbol);
    PyObject *item = NULL;

    if (typ == env->intern(env, "integer")) {
        item = PyInt_FromLong(env->extract_integer(env, symbol));
    } else if (typ == env->intern(env, "float")) {
        item = PyFloat_FromDouble(env->extract_float(env, symbol));
    } else if (typ == env->intern(env, "string")) {
        const char *str = copy_string_from_emacs(env, symbol);
        if (str == NULL)
            goto error;
        item = PyString_FromString(str);
    } else if (typ == env->intern(env, "vector")) {
        ptrdiff_t size = env->vec_size(env, symbol);
        item = PyList_New(size);
        if (item == NULL)
            goto error;

        for (ptrdiff_t i = 0; i < size; i++) {
            PyObject *list_item = convert_emacs_symbol(
                env, env->vec_get(env, symbol, i));
            PyList_SetItem(item, i, list_item);
        }
    } else if (typ == env->intern(env, "user-ptr")) {
        item = (PyObject *)(env->get_user_ptr(env, symbol));
    } else {
        set_error("Unknown datatype"); /* FIXME: Can we get a string rep of the type? */
        return NULL;
    }

    return item;

error:
    py_err();
    return NULL;
}


/* Convert an array of Emacs symbols to a Python tuple, by calling
 * convert_emacs_symbol for each element in the array. */
static PyObject*
convert_from_emacs(emacs_env *env, ptrdiff_t nargs, emacs_value *args)
{
    PyObject *args_tuple = PyTuple_New(nargs);
    if (args_tuple == NULL) {
        set_error("Could not allocate arguments tuple");
        return NULL;
    }

    for (size_t i = 0; i < nargs; i++) {
        PyObject *arg = convert_emacs_symbol(env, args[i]);
        if (arg == NULL) {
            Py_DECREF(args_tuple);
            return NULL;
        }
        Py_INCREF(arg);
        PyTuple_SetItem(args_tuple, i, arg);
    }

    return args_tuple;
}


/* Decrease refence count. This is a wrapper around the equivalent Python
 * macro, used as a callback by Emacs to clean up the data underlying a
 * user-ptr. */
static void
_PyObject_DecRef(void *obj)
{
    Py_XDECREF(obj);
}

/* Convert a Python object to an equivalent Emacs object. If the conversion
 * cannot be done in a meaningful way, return a user-ptr. */
static int
convert_to_emacs(emacs_env *env, emacs_value* e_retval, PyObject *retval)
{
    *e_retval = NULL;

    /* Basic types */
    if (retval == Py_None) {
        *e_retval = env->intern(env, "nil");
    } else if (PyObject_TypeCheck(retval, &PyInt_Type)) {
        *e_retval = env->make_integer(env, PyInt_AsLong(retval));
    } else if (PyObject_TypeCheck(retval, &PyFloat_Type)) {
        *e_retval = env->make_float(env, PyFloat_AsDouble(retval));
    } else if (PyObject_TypeCheck(retval, &PyString_Type)) {
        *e_retval = env->make_string(env, PyString_AsString(retval),
                                     PyString_Size(retval));
    } else {
        /* Check for list type */
        Py_ssize_t len = PyObject_Length(retval);
        if (len != -1) {
            emacs_value Flst = env->intern(env, "list");
            emacs_value *args = malloc(len * sizeof(emacs_value));
            if (args == NULL) {
                set_error("Malloc failed");
                return -1;
            }

            for (Py_ssize_t i = 0; i < len; i++) {
                PyObject *index = PyInt_FromLong(i);
                PyObject *item = PyObject_GetItem(retval, index);
                Py_DECREF(index);

                convert_to_emacs(env, &args[i], item);
                Py_DECREF(item);
            }
            *e_retval = env->funcall(env, Flst, len, args);
            free(args);
        } else {
            /* Make a user pointer and return an opaque Python object. */
            Py_INCREF(retval);
            *e_retval = env->make_user_ptr(env, _PyObject_DecRef, retval);
        }
    }

    if (e_retval == NULL) {
        set_error("Could not convert Python object to emacs");
        return -1;
    }

    return 0;
}


static emacs_value
Fpython_funcall(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    PyObject *py_args = NULL, *py_retval = NULL;

    const char *name = copy_string_from_emacs(env, args[0]);
    if (name == NULL)
        goto error;

    py_args = convert_from_emacs(env, nargs - 1, args + 1);
    if (py_args == NULL)
        goto error;

    py_retval = call_python_function(name, py_args);
    if (py_retval == NULL)
        goto error;

    emacs_value retval;
    if (convert_to_emacs(env, &retval, py_retval) < 0)
        goto error;

    Py_DECREF(py_retval);
    Py_DECREF(py_args);
    return retval;

error:
    Py_XDECREF(py_args);
    Py_XDECREF(py_retval);
    EMACS_HANDLE_ERROR(env);
}


static emacs_value
Fpython_exec(emacs_env *env, ptrdiff_t nargs, emacs_value args[], void *data)
{
    const char *code = copy_string_from_emacs(env, args[0]);
    if ((code == NULL) || (exec_python_code(code) < 0))
        EMACS_HANDLE_ERROR(env);

    return env->intern(env, "t");  /* Succes */
}


/* Bind NAME to FUN.  */
static void
bind_function(emacs_env *env, const char *name, emacs_value Sfun)
{
    /* Set the function cell of the symbol named NAME to SFUN using
       the 'fset' function.  */

    /* Convert the strings to symbols by interning them */
    emacs_value Qfset = env->intern(env, "fset");
    emacs_value Qsym = env->intern(env, name);

    /* Prepare the arguments array */
    emacs_value args[] = { Qsym, Sfun };

    /* Make the call (2 == nb of arguments) */
    env->funcall(env, Qfset, 2, args);
}


/* Provide FEATURE to Emacs.  */
static void
provide (emacs_env *env, const char *feature)
{
    /* call 'provide' with FEATURE converted to a symbol */

    emacs_value Qfeat = env->intern (env, feature);
    emacs_value Qprovide = env->intern (env, "provide");
    emacs_value args[] = { Qfeat };

    env->funcall (env, Qprovide, 1, args);
}


int
emacs_module_init (struct emacs_runtime *ert)
{
    emacs_env *env = ert->get_environment (ert);

    init_python_environment();
    if (module == NULL) {
        /* FIXME: don't rely on global module */
        return -1;
    }

#define DEFUN(lsym, csym, amin, amax, doc, data) \
    bind_function(env, lsym, \
                  env->make_function (env, amin, amax, csym, doc, data))

    DEFUN("python-funcall", Fpython_funcall, 0, 32,
          "Call a Python function in the embedded interpreter.",
          NULL);

    DEFUN("python-exec", Fpython_exec, 1, 1,
          "Execute Python code in the embedded interpreter.", NULL);

#undef DEFUN

    provide (env, "emacs-python");
    return 0;
}

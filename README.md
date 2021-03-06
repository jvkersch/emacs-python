Emacs-Python: a Python module for Emacs
=================================

Emacs-Python is a very lightweight bridge between Emacs 25 and Python. It provides an
Emacs extension module that runs the Python interpreter inside Emacs, so that
Python functions can be run transparently from within Emacs.

Requirements
------------

Emacs-Python requires a recent build of Emacs (>= 25.1) *with module support
enabled*. Most likely, this will involve compiling Emacs from source with the
`--with-modules` flag.

Emacs-Python also requires Python (2.7; Python 3 support is in the works).

Building
--------

To build, run `make`. If there are no errors, you should have a file
`emacs-python.so` in the `src/` folder.

If you want to build against a non-standard Python interpreter, make sure that
the `python-config` script refers to that interpreter before building (by
editing the Makefile if necessary).

Walkthough
----------

To use, copy the `emacs-python.so` library to a directory that's on the Emacs `load-path`.

From within Emacs, run
```elisp
(require 'emacs-python)
```

To send a Python snippet to the embedded interpreter, use `python-exec`:
```elisp
(python-exec "
def foo():
   return 42
")
```

To call a Python function from within Emacs, use `python-funcall`:
```elisp
(python-funcall "foo") ;; outputs 42

(python-funcall "range" 3 7) ;; outputs (3 4 5 6)
```

The return value of Python function calls will be serialized into an equivalent
Emacs type. If there's no meaningful corresponding return type, Emacs will
return a user-ptr.

License
-------
Emacs-Python is released under the [GPL, version 3.0](http://www.gnu.org/licenses/gpl-3.0.en.html).

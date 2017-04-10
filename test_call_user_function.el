(add-to-list 'load-path ".")
(require 'emacs-python)

(python-exec "
def fun_noargs():
    return 42
")
(ert-deftest test-call-userdef-function ()
  (should
   (=
    (python-funcall "fun_noargs")
    42)))

(python-exec "
def fun_someargs(x, y, z):
    return x + y * z
")
(ert-deftest test-call-userdef-function-args ()
  (should
   (=
    (python-funcall "fun_someargs" 3 4 5)
    23)))

(python-exec "
def fun_that_raises():
    raise RuntimeError('I come from Python')
")
(ert-deftest test-call-userdef-raises ()
  (should
   (equal
    (should-error
     (python-funcall "fun_that_raises"))
    '(error . "Python error: I come from Python"))))

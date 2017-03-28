(add-to-list 'load-path ".")
(require 'pymacs)

;; Run the same test a number of times in a loop. This is good to detect memory
;; issues; double free/Py_DECREF will usually cause this to segfault.

(python-exec "
def fun(x):
    return x**2
")
(ert-deftest test-call-function-repeat ()
  (let ((count 0))
    (while (< count 1000)
      (should
       (=
        (python-funcall "fun" count)
        (* count count)))
      (setq count (1+ count)))))

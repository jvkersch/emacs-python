(add-to-list 'load-path ".")
(require 'pymacs)

(ert-deftest test-call-builtin-len-function ()
  :tags '(run-builtin-len)
  (should (equal (python-funcall "len" [1 2 3 4]) 4)))

(ert-deftest test-call-builtin-rangefunction ()
  :tags '(run-builtin-range)
  (should (equal (python-funcall "range" 5) '(0 1 2 3 4))))

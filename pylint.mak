PYTHONPATH1      = $(abs_top_srcdir)/python-pkg/lirc:
PYTHONPATH2      = $(abs_top_srcdir)/python-pkg/lirc/lib/.libs
PYTHONPATH       = $(PYTHONPATH1):$(PYTHONPATH2)
PYLINT           = python3-pylint
pylint_template  = {path}:{line}: [{msg_id}({symbol}), {obj}] {msg}

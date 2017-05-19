#include <Python.h>
#include "lirc_client.h"

/**
 *  @file _client.c
 *  @author Alec Leamas
 *  @brief python bindings for part of lirc_client.h
 *  @ingroup  python_bindings
 *
 *  These bindings provides python3 access to some of the lirc_client.h functions.
 */


static const char* const LIRCRC_ID = "lircd.lircrc";


/** @see: lirc_init() */
static PyObject* client_lirc_init(PyObject *self, PyObject *args)
{
	const char* prog;
	int r;
	PyObject* intval;

	if (!PyArg_ParseTuple(args, "s", &prog))
		return NULL;
	r = lirc_init(prog, 1);
	if (r == -1) {
		PyErr_SetString(PyExc_RuntimeError,
				"Cannot open lircd socket");
		return NULL;
	}
	intval = Py_BuildValue("i", r);
	return intval;
}


/** @see: lirc_deinit() */
static PyObject* client_lirc_deinit(PyObject *self, PyObject *args)
{
	int r;

	r = lirc_deinit();
	if (r == -1)
		return PyErr_SetFromErrno(PyExc_OSError);
	return Py_BuildValue("i", r);
}


/** @see: lirc_readconfig() */
static PyObject* client_lirc_readconfig(PyObject *self, PyObject *args)
{
	const char* path;
	struct lirc_config* config;
	PyObject* lircrc;
	char text[128];

	if (!PyArg_ParseTuple(args, "s", &path))
		return NULL;
	if (access(path, R_OK) != 0) {
		snprintf(text, sizeof(text), "Cannot open %s for read", path);
		PyErr_SetString(PyExc_RuntimeError, text);
		return NULL;
	}
	if (lirc_readconfig(path, &config, NULL) != 0) {
		PyErr_SetString(PyExc_RuntimeError,
				"Cannot parse lircrc file");
		return NULL;
	}
	lircrc = PyCapsule_New(config, LIRCRC_ID, NULL);
	return lircrc;
}


/** @see: lirc_freeconfig() */
static PyObject* client_lirc_freeconfig(PyObject *self, PyObject *args)
{
	PyObject* lircrc;
	struct lirc_config* config;

	if (!PyArg_ParseTuple(args, "O", &lircrc))
		return NULL;
	config = PyCapsule_GetPointer(lircrc, LIRCRC_ID);
	lirc_freeconfig(config);
	Py_RETURN_NONE;
}


/**
 * @return: A possibly empty list of all decoded items.
 * @see: lirc_code2char().
 */
static PyObject* client_lirc_code2char(PyObject *self, PyObject *args)
{
	PyObject* lircrc;
	char* program;
	char* code;

	struct lirc_config* config;
	char* s;
	int i;
	int r;
	PyObject* list = PyList_New(0);
	PyObject* string;

	if (!PyArg_ParseTuple(args, "Oss", &lircrc, &program, &code)) {
		PyErr_SetString(PyExc_RuntimeError, "Cannot parse arguments");
		return NULL;
	}
	config = PyCapsule_GetPointer(lircrc, LIRCRC_ID);
	for (i = 0; i < 10; i += 1) {
		r = lirc_code2char(config, code, &s);
		if (r != 0 || s == NULL || !*s)
			break;
		string = Py_BuildValue("s", s);
		if (!string) {
			PyErr_SetString(
				PyExc_RuntimeError, "Cannot decode string");
			Py_DECREF(list);
			return NULL;
		}
		if (PyList_Append(list, string) == -1) {
			PyErr_SetString(PyExc_RuntimeError,
					"Cannot append string");
			Py_DECREF(string);
			Py_DECREF(list);
			return NULL;
		}
		Py_DECREF(string);
	}
	return list;
}


PyMethodDef ClientMethods[] = {
	{"lirc_init",  client_lirc_init, METH_VARARGS,
		"initiate lirc connection"},
	{"lirc_deinit",  client_lirc_deinit, METH_VARARGS,
		"close lirc connection opened by lirc_init()"},
	{"lirc_readconfig",  client_lirc_readconfig, METH_VARARGS,
		"parse a lircrc file"},
	{"lirc_freeconfig",  client_lirc_freeconfig, METH_VARARGS,
		"Deallocate memory obtained using lirc_readconfig()"},
	{"lirc_code2char", client_lirc_code2char, METH_VARARGS,
		"lircrc-translate a keypress"},
	{0}
};


static struct PyModuleDef clientmodule = {
	PyModuleDef_HEAD_INIT,
	"_client",
	NULL,		/* module documentation, may be NULL */
	-1,
	ClientMethods
};


PyMODINIT_FUNC PyInit__client(void)
{
	return PyModule_Create(&clientmodule);
}

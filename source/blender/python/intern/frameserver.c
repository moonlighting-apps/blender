/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Benoit Bolsee.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/frameserver.c
 *  \ingroup pythonintern
 *
 * This file defines the 'frameserver' module, used to get data from the 
 * requests on the framserver
 */

/* python redefines */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include <Python.h>

#include "BLI_utildefines.h"
#include "BKE_writeframeserver.h"

#include "frameserver.h"

PyDoc_STRVAR(M_frameserver_doc,
    "This module provides access to the frameserver module"
);

static struct PyModuleDef frameserver_module = {
    PyModuleDef_HEAD_INIT,
    "frameserver",          /* Name of the module */
    M_frameserver_doc,      /* module documentation */
    -1,                     /* size of per-interpreter state of the module,
                             * or -1 if the module keeps state in global variables */
    NULL, NULL, NULL, NULL, NULL
};

static PyObject *PyInit_frameserver(void)
{
    PyObject *m;

    m = PyModule_Create(&frameserver_module);
    if (m == NULL)
        return NULL;

    /* Module initialization */
    return m;
}


/* dummy function */
PyDoc_STRVAR(FRAMESERVER_get_path_doc,
    "Get a the params form a request of a new server"
);
PyDoc_STRVAR(FRAMESERVER_start_server_doc,
    "Start the server initializing the sockets."
);
PyDoc_STRVAR(FRAMESERVER_stop_server_doc,
    "Start the server initializing the sockets."
);
PyDoc_STRVAR(FRAMESERVER_only_one_frame_doc,
    "Set a flag if the frameserver will serve one frame per request. Returns True on success, False otherwise"
);

static PyObject *FRAMESERVER_get_path(PyObject *UNUSED(self), PyObject *UNUSED(args))
{
    char path[4096];
    PyObject *obj;

    BKE_frameserver_get_changes(path);
    obj = Py_BuildValue("s", path);

    return obj;
}

static PyObject *FRAMESERVER_start_server(PyObject *UNUSED(self), PyObject *UNUSED(args))
{
    int started = BKE_server_start(NULL);
    PyObject *obj = Py_BuildValue("d", started);

    return obj;
}

static PyObject *FRAMESERVER_stop_server(PyObject *UNUSED(self), PyObject *UNUSED(args))
{
    BKE_frameserver_end();
    Py_RETURN_NONE;
}

static PyObject *FRAMESERVER_only_one_frame(PyObject *UNUSED(self), PyObject *args)
{
    int param;
    if (!PyArg_ParseTuple(args, "i", &param)) {
        Py_RETURN_FALSE;
    }
    BKE_frameserver_only_one_frame(param);
    Py_RETURN_TRUE;
}


static PyMethodDef py_methods[] = {
    {"get_path", (PyCFunction)FRAMESERVER_get_path, METH_NOARGS, FRAMESERVER_get_path_doc},
    {"only_one_frame", (PyCFunction) FRAMESERVER_only_one_frame, METH_VARARGS, FRAMESERVER_only_one_frame_doc},
    {"start_server", (PyCFunction)FRAMESERVER_start_server, METH_NOARGS, FRAMESERVER_start_server_doc},
    {"stop_server", (PyCFunction)FRAMESERVER_stop_server, METH_NOARGS, FRAMESERVER_stop_server_doc}
};

PyObject *FS_initPython(void)
{
    PyMethodDef *meth_def = NULL;
    PyObject *module = PyInit_frameserver();
    PyModule_AddObject(module, "get_path", (PyObject *)PyCFunction_New(py_methods, NULL));
    PyModule_AddObject(module, "only_one_frame", (PyObject *)PyCFunction_New(&py_methods[1], NULL));
    PyDict_SetItemString(PyImport_GetModuleDict(), "frameserver", module);

    return module;
}

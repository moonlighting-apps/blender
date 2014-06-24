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
    "Get a dummy path"
);

static PyObject *FRAMESERVER_get_path(PyObject *UNUSED(self), PyObject *UNUSED(args))
{
    char *path = BKE_frameserver_get_changes();
    PyObject *obj = Py_BuildValue("s", path);
    free(path);
    return obj;
}

static PyMethodDef meth_get_path[] = {
    {"get_path", (PyCFunction)FRAMESERVER_get_path, METH_NOARGS, FRAMESERVER_get_path_doc}
};

PyObject *FS_initPython(void)
{
    PyObject *module = PyInit_frameserver();
    PyModule_AddObject(module, "get_path", (PyObject *)PyCFunction_New(meth_get_path, NULL));
    PyDict_SetItemString(PyImport_GetModuleDict(), "frameserver", module);

    return module;
}

############################################################################
# tools/gdb/nuttx_gdb/__init__.py
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

import importlib
from os import path

import gdb

here = path.dirname(path.abspath(__file__))


def register_commands(event):
    if getattr(register_commands, "registered", False):
        return

    register_commands.registered = True

    gdb.write(f"Registering NuttX GDB commands from {here}\n")
    gdb.execute("set pagination off")
    gdb.write("set pagination off\n")
    gdb.execute("set python print-stack full")
    gdb.write("set python print-stack full\n")
    gdb.execute('handle SIGUSR1 "nostop" "pass" "noprint"')
    gdb.write('"handle SIGUSR1 "nostop" "pass" "noprint"\n')

    def init_gdb_commands(m: str):
        module = importlib.import_module(f"{__package__}.{m}")
        for c in module.__dict__.values():
            if isinstance(c, type) and issubclass(c, gdb.Command):
                c()

    # import utils module
    utils = importlib.import_module(f"{__package__}.utils")
    modules = utils.gather_modules(here)

    # Register prefix commands firstly
    init_gdb_commands("prefix")
    modules.remove("prefix")
    modules.remove("__init__")

    # Register all other modules
    for m in modules:
        init_gdb_commands(m)

    utils.check_version()


if len(gdb.objfiles()) == 0:
    gdb.events.new_objfile.connect(register_commands)
else:
    register_commands(None)

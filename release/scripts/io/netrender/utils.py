# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
import sys, os
import re
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.model

VERSION = b"0.5"

# Jobs status
JOB_WAITING = 0 # before all data has been entered
JOB_PAUSED = 1 # paused by user
JOB_FINISHED = 2 # finished rendering
JOB_QUEUED = 3 # ready to be dispatched

# Frames status
QUEUED = 0
DISPATCHED = 1
DONE = 2
ERROR = 3

STATUS_TEXT = {
								QUEUED: "Queued",
								DISPATCHED: "Dispatched",
								DONE: "Done",
								ERROR: "Error"
							}

def rnaType(rna_type):
	bpy.types.register(rna_type)
	return rna_type

def rnaOperator(rna_op):
	bpy.ops.add(rna_op)
	return rna_op

def clientConnection(scene):
		netsettings = scene.network_render
		
		if netsettings.server_address == "[default]":
			bpy.ops.render.netclientscan()
		
		conn = http.client.HTTPConnection(netsettings.server_address, netsettings.server_port)
		
		if clientVerifyVersion(conn):
			return conn
		else:
			conn.close()
			return None

def clientVerifyVersion(conn):
	conn.request("GET", "/version")
	response = conn.getresponse()
	
	if response.status != http.client.OK:
		conn.close()
		return False
	
	server_version = response.read()
	
	if server_version != VERSION:
		print("Incorrect server version!")
		print("expected", VERSION, "received", server_version)
		return False
	
	return True

def prefixPath(prefix_directory, file_path, prefix_path):
	if os.path.isabs(file_path):
		# if an absolute path, make sure path exists, if it doesn't, use relative local path
		full_path = file_path
		if not os.path.exists(full_path):
			p, n = os.path.split(full_path)
			
			if prefix_path and p.startswith(prefix_path):
				directory = prefix_directory + p[len(prefix_path):]
				full_path = directory + os.sep + n
				if not os.path.exists(directory):
					os.mkdir(directory)
			else:
				full_path = prefix_directory + n
	else:
		full_path = prefix_directory + file_path
	
	return full_path

# gphotofs2
a rewrite of gphotofs

## Usage
<pre>
mkdir build
cd build
cmake ..
make
./gphotofs2 &lt;options> &lt;mount point>
</pre>

## Why rewrite
gphotofs has several problems:
* copy something to the MTP device does not save data

File is created, but length is 1
* does not support truncate

"Function is not implemented" is frequently seen.
* FS stat reporting is incorrect.

statvfs.f_frsize is not set. The default is 512, which should be 1024.

And I don't like the hashtable-based design.

## Why not anything based on libmtp (mtpfs, ...)
Currently, libmtp does not support progressive loading.
You need to wait for a long time when you connect a new device.

## Design
Files and directories are represented by objects, organized in a tree.
Cache the directory info and file info in the memory.
Load directory info progressively.
Buffer file contents in the memory, flush during close().

## TODO
* Better locking.
* Free buffers when all handles are closed.

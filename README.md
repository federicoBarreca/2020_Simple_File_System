# SO-2020 FileSystem

Project no. 5: File System implemented using LLA by Federico Barreca

The file system reserves the first part of the file to store:

- A linked list of free blocks
- Linked lists of file blocks
- A single global directory

Blocks are of two types:

- Data blocks: contain "random" information (i.e. actual data of a file)
- Directory blocks: contain a sequence of structs of type directory_entry,
containing the blocks where the files in that folder start and
whether or not those are directory themselves.

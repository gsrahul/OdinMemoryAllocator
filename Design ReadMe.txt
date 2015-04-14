This memory allocator is part of a game engine named Odin which I am working on in my spare time. This allocator was designed to be portable across platforms and minimize fragmentation as much as possible by breaking down allocations based on usage patterns. Certain parts of the design have been inspired from other online blogs on game engines such as MolecularEngine and BitSquid engine.

Design:
There are three different types of allocators derived from a common Allocator base class:
1) Linear Allocator
Used for allocations that last the entire lifetime of the application. Can also be used as a Frame allocator for allocations lasting only one frame.

2) Pool Allocator
Used for multiple allocations of the same type.

3) General purpose Allocator
This allocator is used for general purpose allocations of objects. One dlmalloc instance for allocations larger than 256 bytes and 20 dsmalloc instances covering allocations at every 8 byte size interval less than 64 bytes and every 16 byte interval between 64 and 256 bytes.  The instance for large allocations uses a 32 megabyte segment size and 64 kilobyte pages whereas the small allocation instances use 64 kilobyte segments. These dlmalloc instances aggressively return memory to the system when any instance or a segment of an instance is not being used. This design is based on the F.E.A.R 3 memory allocator.

Virtual memory:
All the above allocators fully support virtual memory and memory is requested from the OS at page granularity. Also, the system level functions used to allocate virtual memory can easily be changes based on flags to port it to a different OS (currently only system calls for Windows are added).
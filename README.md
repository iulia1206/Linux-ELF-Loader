This is the implementation of a signal handler that treats SIGSEGV.

I began the implementation by creating a default handler to use
when there was not a situation where I had to alloc memory for the 
page.

I passed through all segments and if I did not find the address where the
executable crashed, I called the default handler. Then, I noted which pages
were mapped and which were not.

So, I began reading the corresponding page from the exec file into a buffer.
I checked how much to read from that page, if there were any bytes left to 
read, if there was a full page or if it was less then a page (in the last case
I had to set the remaining memory to a full page to 0). Using the read_x_bytes
function, I read the data from the exec file path in the buffer. Next, I 
mapped a page into memory, copied the buffer to that page and assigned
it the right permissions from the so_seg_t field. After I finished completing 
the page, I marked it as mapped in the data field. 

If I was on a page that was already mapped, I called the default handler.

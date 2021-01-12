We used buckets as well as thread local storage 
to make sure that each thread has it's own instance and they 
do not access the same bins.
 
We store any leftover mapped memory in an array of 
free_list_cells (buckets), and then traverse this array when we 
are trying to allocate any memory less than 2048 (the max
size for our buckets). 

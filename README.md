# HTTP-Based Key-Value Server
This is an implementation of a key-value server which stores image names as keys, and values as images. The server serves two purpose: to store and retrieve images, and rotate image by a user-specified angle.

# Installation
To install, first install dependencies in docs/requirements.txt.  
Then do make.

# Description and Usage
For a client, 
key is a string, value is the path to an image (only jpg/jpeg images allowed).  
key can be simply the name of the image. A key is unique in the database, so one key can only correspond to a single image.

[create] should store the image to the database with key k. (IO bound)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;    create &lt;key&gt; &lt;path to image&gt;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;    create 000.jpg img/african_elephant/000.jpg  

[read] should return the image saved on database  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   read &lt;key&gt;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   read 000.jpg  

[delete] should delete the image from the database  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   delete &lt;key&gt;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   delete 000.jpg  

[rotate] the image sent to the server by some degrees (integer), and then return the image to the client. Does not save the image on database. (leads to CPU bound process)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   rotate &lt;angle degrees&gt; &lt;path to image&gt;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   rotate 45 img/african_elephant/000.jpg

[rotate2] takes the image &lt;key&gt; from database, rotates it and saves it in the database. Does not return the image to the user.  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   rotate2 &lt;angle degrees&gt; &lt;key&gt;  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;   rotate2 45 000.jpg

# Notes

I should be able to generate two different workloads, one that is CPU bound, and other that is I/O bound.  
1. Uploading an image to a server (IO bottleneck in the database server).
2. Rotate an image in the server (that user sends) and send it as a response (CPU bottleneck in the server).

cpp-httplib uses a thread-pool by default, written here https://github.com/yhirose/cpp-httplib/issues/415.

What does eviction mean in case of hash-table? When the hash-map has CACHE_SIZE number of elements, we will delete an element
from it. Choosing the FCFS replacement policy because it is easy to implement. We need to store separately the order in
which the keys arrive (stored in queue_of_keys queue).

Since it is a key-value type data, relational databases may not be suitable here. So using NoSQL database Apache Cassandra (free and open source).  
Installing instructions:  https://cassandra.apache.org/doc/latest/cassandra/installing/installing.html.
If cassandra hangs the system, then the OOM may be killing cassandra because it is demanding too much heap. Reduce its max heap size (use gpt).  
Cassandra takes 20 to 60 seconds to load after boot, so wait.

Server will connect to the database using http-based requests via tcp connection.  

Each process has been pinned to a separate core for load testing purpose.

Do 'sudo systemctl start cassandra' before running database, wait for a minute for cassandra to start.
 
### Dependencies
1. cpp-httplib (for http request/response)
2. Apache cassandra (for database)
3. opencv (used for rotating images)

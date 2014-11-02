udptransmitter
==============

=== Server ===

Socket Info Structures And get_ifi_info_plus.c : 
We used the code provided in the get_ifi_info_plus.c file. This gave us the ability to walk
over all our interface that we are attached to. For each of these interfaces we set up a 
listening socket. We of course skipped the broadcast address as specified by the book. For 
each interface that we found we defined a data struct called socketInfo. Inside this struct 
we stored the socket file descriptor, the in_addr structuress for the ip addres, the network 
mask and the  subnet mask. In addition we also had a reference to the socket itself incase 
we needed it else where. These structures made it easier to manage all the information to 
calculate longest prefix matching and if the client was local or not detection. 

RTO Modifications for section 22.5 code:
TODO:: ROMAN FILL IN PLEASE


ARQ Mechanisms:
Reliable Data transfer: We have implemented a circular buffer on both the client and the server.
This is a doubly linked circular linked list that holds the information about our intransit 
packets on the server side. And on the client side it is a similar doubly linked list that buffers
the packets it has received. It maintains pointers for the segments that are inorder such that the
consumer can wake up and consume all the buffer space in order. The client always acks the last inorder
segment that it has received.

Flow control:
Out TCP header that we defined has a field that the client populates with every ACK that it sends. This 
field contains the info about how many buffer slots the client has open at that present moment. When
the server receives the ACK, he parses sets the client's receiving window to this freshly update value 
that it just received. Before sending packets, the server takes the minimum across its cwin, client's 
advertised window, and its available space in the buffer. This allows the server never to overwhelm the 
clients receive buffer. 


Slow Start:
Cwin = 1 at the start and ssthresh equals the clients buffer space to start of with. While cwin is less
than ssthresh, for every ack we get back we increment 






=== Client ===

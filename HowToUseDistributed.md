# Running the Distributed Search Engine
## Step 1
Build the binaries
## Step 2
Make the indexes
## Step 3
Inside /bin directory, there should be a file called mithril_manager.conf; you can use this file to input in the port number you want to host the backend server on as well as the index paths you want to use for this machine. Note that the port number must come first, and you can add as many index directories as you would like. Furthermore, lines starting with # will be ignored as comments.

*./mithril_manager --conf mithril_manager.conf*

**Note: if running multiple mithril_manager processes on one machine (ie. for testing), you can explicitly pass in port num and index path as follows**

*./mithril_manager --port {port_num} --index {index1} {index2} ...*

## Step 4
On the query server, edit the servers.conf file to contain the IPs and Ports the backend indexes are running on. run either the frontend server or mithril_coordinator binary.

*mithril_coordinator --conf servers.conf*

## Step 5
Search the query and you should get results from each sever
this is the clone for sysrc from FreeBSD written in C 
it edits/modifies the /etc/rc.conf
for example:
sysrc hostname=teste.mydomain domainname=mydomain => it modifies /etc/rc.conf according
sysrc -x ypbind => removes the ypbind from /etc/rc.conf

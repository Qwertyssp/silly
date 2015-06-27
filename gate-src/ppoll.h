#ifndef _PPOLL_H
#define _PPOLL_H

int ppoll_init();
void ppoll_exit();

int ppoll_listen(int port);

const char *ppoll_pull(int *fd);
void ppoll_push();


#endif
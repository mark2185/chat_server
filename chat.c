#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <syslog.h>
#include <err.h>

#define STDIN 0
#define MAX_CLIENTS 5

void print_usage() 
{
    printf("chat [-t tcp_port] [-u udp_port] [-k kontrolni_port lozinka]\n");
}

void print_sockets(int *s)
{
    printf("Socket descriptors are: %d %d %d %d %d\n", s[0], s[1], s[2], s[3], s[4]);
}

void broadcast_msg(int *socks, char *msg, int exclude_sock)
{
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (i == exclude_sock)
            continue;
        if (socks[i] != 0) {
            send(socks[i], msg, strlen(msg), 0);
        }
    }
}

int create_ctl_sock(int *sock, char *port)
{
    struct sockaddr_in address;
    *sock = socket(AF_INET, SOCK_DGRAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(port));

    return bind(*sock, (struct sockaddr *)&address, sizeof(address));
}

int chat_unlock(char *password, char *msg)
{
    printf("Trying to unlock\n");
    char keyword[1024] = {0};
//    strcpy(keyword, "ON:");
    strcat(keyword, "ON:");
//    printf("asdf");
    strcat(keyword, "asdf");

    printf("Msg je %s", msg);
    return (strcmp(msg, keyword) == 0);
}

int chat_lock(char *password, char *msg)
{
    char keyword[1024]={0};
    printf("Trying to unlock\n");
    strcat(keyword, "OFF:");
    strcat(keyword, password);

    return (strcmp(msg, keyword) == 0);

}
int chat_quit(int *clients, char *password, char *msg)
{
	char keyword[1024]={0};
	strcat(keyword, "QUIT:");
	strcat(keyword, password);	

	if (!strcmp(msg, keyword)) {

		for (int i=0; i<MAX_CLIENTS; i++) {
			if (clients[i] == 0)
				continue;
			close(clients[i]);
			clients[i] = 0;
		}
		return 1;
	}
	return 0;
}

int main(int argc, char* argv[]) 
{
    char *tcp_port = "1234";
    char *udp_port = "1234";
    char *ctl_port = "1234";
    int master_socket, dgram_sock, ctl_sock, client_socket[MAX_CLIENTS]={0};
    int daemon_flag=0;
    int ch;
    int chat_locked=0;
    int opt=1;
    int i, error;
    int addrlen, readbytes;
    int activity, new_socket;
    int sd, max_sd;

    struct sockaddr_in address;

    char buffer[1025]={0};
    char msg[1025]={0};
    char *password;
    char *err_msg;
    char ctl_msg[1025]={0};
    char tmp[1024]={0};

    fd_set readfds;

    while((ch=getopt(argc, argv, "t:u:k:")) != -1) {
        if (argc == 1) 
            break;
        switch(ch) {
        case 't':
            tcp_port = optarg;
            break;
        case 'u':
            udp_port = optarg;
            break;
        case 'k':
            ctl_port = argv[optind-1];
            password = argv[optind];
//	        daemon(0, 0);
            openlog("lm48711:MrePro chat", LOG_PID, LOG_FTP);
            daemon_flag = 1;

            if (create_ctl_sock(&ctl_sock, ctl_port) < 0){
                err_msg = "Failed to create the control UDP socket!\n";
                syslog(LOG_ALERT, "Failed to create the control UDP socket!\n");
                return 1;
            }
            chat_locked=1;
            break;
        default:
            print_usage();
            return 1;
        }
    }


    while(chat_locked && daemon_flag) {
	    readbytes = read(ctl_sock, ctl_msg, 1024); 

        ctl_msg[readbytes-1]=0;
        strcpy(tmp, "ON:");
        strcat(tmp, password);
        chat_locked = strcmp(tmp, ctl_msg);
        memset(tmp, 0, 1024);
    }

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        err_msg = "Error creating master socket!\n";
        if (daemon_flag) {
            syslog(LOG_ALERT, "Error creating master socket!\n");
            return 1;
        } else {
            errx(1, "Error creating master socket!\n");
        }
    }

    if ((dgram_sock = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        err_msg = "Error creating UDP socket!\n";
        if (daemon_flag) {
            syslog(LOG_ALERT, "Error creating UDP socket!\n");
            return 1;
        } else {
            errx(1, "Error creating UDP socket!\n");
        }
    }


    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) { 
        printf("Error setting master socket options!\n");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(tcp_port));

    if ((error = bind(master_socket, (struct sockaddr *)&address, sizeof(address))) < 0) {
        err_msg = "Error binding TCP socket!\n";
        if (daemon_flag) {
            syslog(LOG_ALERT, err_msg);    
            return 1;
        } else {
            errx(error, err_msg);
        }
    }
    
    address.sin_port = htons(atoi(udp_port));
    
    if ((error = bind(dgram_sock, (struct sockaddr *)&address, sizeof(address))) < 0) {
        err_msg = "Error binding UDP socket!\n";
        if (daemon_flag) {
            syslog(LOG_ALERT, err_msg);
            return error;
        } else {
            errx(1, err_msg);
        }
    }
    
    printf("Started listening on port %s...\n", tcp_port);

    if ((error = listen(master_socket, MAX_CLIENTS)) < 0) {
        err_msg = "Listening error!\n";
        if (daemon_flag) {
            syslog(LOG_ALERT, err_msg);
            return error;
        }
        errx(error, err_msg);
    }

    while(1) {
        memset(buffer, 0, 1025);
        memset(msg, 0, 1025);
        memset(tmp, 0, 1024);
        FD_ZERO(&readfds);
        if (daemon_flag)
        	FD_SET(ctl_sock, &readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(master_socket, &readfds);
        if (dgram_sock)
            FD_SET(dgram_sock, &readfds);

//        printf("Socks: %d %d %d %d\n", ctl_sock, STDIN, master_socket, dgram_sock);
        max_sd = (dgram_sock>master_socket)?dgram_sock:master_socket;
        for (i=0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];

            if (sd > 0) 
                FD_SET(sd, &readfds);

            if (sd > max_sd)
                max_sd = sd;
            
        }
//        printf("Max sd je %d:\n", max_sd);
        activity = select(max_sd+1, &readfds, NULL, NULL, NULL);

        if ((activity<0) && (errno!=EINTR)) {
//            printf("Sockets: %d %d %d \n", ctl_sock, dgram_sock, master_socket);
//            printf("Activity is %d\n")
            err_msg = "Select error!\n";
            if (daemon_flag) {
                syslog(LOG_ALERT, "Select error!\n");
                return activity;
            } else {
                errx(activity, "Select error!\n");
            }
        }

        if (daemon_flag && FD_ISSET(ctl_sock, &readfds)) {
            readbytes = read(ctl_sock, ctl_msg, 1024); 
            ctl_msg[readbytes-1]=0;
            
            if (strstr(ctl_msg, "QUIT:")) {
                strcpy(tmp, "QUIT:");
                strcat(tmp, password);
                if (!strcmp(tmp, ctl_msg)) {
                    for(i=0; i<MAX_CLIENTS; i++) {
                        close(client_socket[i]);
                    }
                }
                return 0;
            }
            
            if (chat_locked && strstr(ctl_msg, "ON:") == 0) {
                strcpy(tmp, "ON:");
                strcat(tmp, password);
                chat_locked = strcmp(tmp, ctl_msg);            

                printf("ON!\n");
                dgram_sock = socket(AF_INET, SOCK_DGRAM, 0);
                bind(dgram_sock, (struct sockaddr *)&address, sizeof(address));
            } else if (!chat_locked && strstr(ctl_msg, "OFF:")){

//                printf("OFF!\n");
                strcpy(tmp, "OFF:");
                strcat(tmp, password);
                chat_locked = !strcmp(tmp, ctl_msg);

                for(i=0; i<MAX_CLIENTS; i++) {
                    close(client_socket[i]);
                    client_socket[i]=0;
                }
                close(dgram_sock);
                dgram_sock = 0;
            }

            continue;

        }

//        if (chat_locked && daemon_flag)
//            continue;

        if (FD_ISSET(dgram_sock, &readfds)) {
            struct sockaddr_in src_addr;
            socklen_t src_addr_len=sizeof(src_addr);
            readbytes = recvfrom(dgram_sock, buffer, 1024, 0, (struct sockaddr*)&src_addr, &src_addr_len);
            buffer[readbytes] = 0;
            sprintf(msg, "UDP:%s:%d", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
            if (daemon_flag) 
                syslog(LOG_INFO, "%s\n", msg);
            else 
                fprintf(stderr, "%s\n", msg);
            strcat(msg, ":");
            strcat(msg, buffer);
            broadcast_msg(client_socket, msg, -1);
            continue;
        }

        if (FD_ISSET(STDIN, &readfds)) {
            char tmp2[1025]={0};
            fgets(tmp2, 1024, stdin);
            sprintf(msg, "Server:%s", tmp2);
            broadcast_msg(client_socket, msg, -1);

           if (daemon_flag)
                syslog(LOG_INFO, "%s\n", msg);

            continue;
        }

        if (FD_ISSET(master_socket, &readfds)) {
            int flag=0;
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))< 0) {
                fprintf(stderr, "Accept error!\n");
                return 1;
            }
            getpeername(new_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            sprintf(msg, "Spojio se:%s:%d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            for (i=0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    flag = 1;
                    client_socket[i] = new_socket;
                    break;
                }
            }
            if (flag) {
                if (daemon_flag) 
                    syslog(LOG_INFO, "%s\n", msg);
                else 
                    fprintf(stderr, "%s\n", msg);
    //               fprintf(stderr, "%s\n", msg);
                strcat(msg, "\n");
                broadcast_msg(client_socket, msg, i);
            }
            continue;
        }

        for (i=0; i<MAX_CLIENTS; i++) {
            sd = client_socket[i];
            
            if (!sd)
                continue;

            if (FD_ISSET(sd, &readfds)) {
                readbytes = read(sd, buffer, 1024);
                getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                if (readbytes == 0) {
                    sprintf(msg, "Odspojio se:%s:%d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    close(sd);
                    client_socket[i]=0;

                    if (daemon_flag) 
                        syslog(LOG_INFO, "%s\n", msg);
                    else 
                        fprintf(stderr, "%s\n", msg);

                    strcat(msg, "\n");
                    broadcast_msg(client_socket, msg, -1);
                } else {
                    buffer[readbytes-1]=0;
                    sprintf(msg, "TCP:%s:%d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    if (daemon_flag) 
                        syslog(LOG_INFO, "%s\n", msg);
                    else 
                        fprintf(stderr, "%s\n", msg);
                    strcat(msg, ":");
                    strcat(msg, buffer);
                    broadcast_msg(client_socket, msg, i);
                }
            }
        }
    }
}


   

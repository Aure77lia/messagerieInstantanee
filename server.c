int main() {
	int sock=socket(PF_INET,SOCK_STREAM,0);
 	struct sockaddr_in address_sock;
 	address_sock.sin_family=AF_INET;
 	address_sock.sin_port=htons(4242);
 	address_sock.sin_addr.s_addr=htonl(INADDR_ANY);
 	int r=bind(sock,(struct sockaddr *)&address_sock,sizeof(struct sockaddr_in));
 	if(r==0){
 		r=listen(sock,0);
 		while(1){
 			struct sockaddr_in caller;
 			socklen_t size=sizeof(caller);
 			int sock2=accept(sock,(struct sockaddr *)&caller,&size);
 			if(sock2>=0){
 				char *mess="Yeah!\n";
 				write(sock2,mess,strlen(mess)*sizeof(char));
 				char buff[100];
 				int recu=read(sock2,buff,99*sizeof(char));
 				buff[recu]='\0';
 				printf("Message recu : %s\n",buff);
 				close(sock2);
 			}
 		}	
 	}
 	return 0;
} 

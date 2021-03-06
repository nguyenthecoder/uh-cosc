#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <sstream>

#define READ_FD 0 
#define WRITE_FD 1 
#define NUM_OCTETS 4
#define NUM_ADDRESS 6

using namespace std;

int share_key  = 200;

const int MAX_STRING_LENGTH = 200;

void split(const string str, string &ip, string &subnet, char delim = ' '){
	std::stringstream ss(str);
	std::string token;
	int index = 0;
	string ip_subnet[2];
	while (std::getline(ss, token, delim)) {
		ip_subnet[index++]= token ;
	}

	ip = ip_subnet[0];
	subnet = ip_subnet[1];
}

void convert_to_int_array(const std::string& str, uint8_t * container, char delim = '.')
{
	std::stringstream ss(str);
	std::string token;
	int index = 0;
	while (std::getline(ss, token, delim)) {
		int number = stoi(token);
		container[index++]= (uint8_t)number ;
	}
}


int8_t get_min_host(uint8_t network, uint8_t & min_host){
	// Returns a carry if there is 
	if(network == 255){
		min_host=0;
		return 1;
	} else {
		min_host =network +  1;
		return 0;
	}
}   

int8_t get_max_host(uint8_t broadcast, uint8_t & max_host){
	// Returns a carry if there is 
	cout<<int(broadcast)<<endl;

	if(broadcast == 0){
		max_host=255;
		return -1;
	} else {
		max_host = broadcast -  1;
		return 0;
	}
}   

int count_zero_bits(const uint8_t n){
	uint8_t count_1 = 0;
	uint8_t temp = n;
	while(temp){
		count_1 += temp & 1;
		temp >>= 1;
	}
	int count_0 = 8 - count_1;
	return count_0;
}

int calculate_number_of_hosts(const uint8_t num_of_zeros){
	int number_of_hosts = pow(2, num_of_zeros) - 2; 
	return number_of_hosts;
}	

void get_network_octet(uint8_t ip, uint8_t subnet, uint8_t &network){
	network = ip & subnet;
}   

void get_broadcast_octet(uint8_t network, uint8_t subnet, uint8_t &broadcast ){
	broadcast =  network | ~subnet; 
}   

void calcualte_network_thread(uint8_t * ip,uint8_t * subnet, uint8_t * network){
	thread threads[NUM_OCTETS];

	for(int i = 0; i< NUM_OCTETS;i++){
		//thread temp = threads[i];
		threads[i] = thread(get_network_octet, ip[i], subnet[i], ref(network[i]));
	}

	for(int i = 0; i< NUM_OCTETS;i++){
		threads[i].join();
	}


}

void calcualte_broadcast_thread(uint8_t * network,uint8_t * subnet, uint8_t * broadcast){
	thread threads[NUM_OCTETS];

	for(int i = 0; i< NUM_OCTETS;i++){
		threads[i] = thread(get_broadcast_octet, network[i], subnet[i], ref(broadcast[i]));
	}

	for(int i = 0; i< NUM_OCTETS;i++){
		threads[i].join();
	}
}

void calculate_min_host(uint8_t * network, uint8_t * min_host){
	uint8_t carry = 1;
	for(int i = NUM_OCTETS - 1; i>=0;i--){
		if(carry==1){
			carry = get_min_host(network[i], min_host[i]);
		} else {
			min_host[i] = network[i];
		}
	}
} 

void calculate_max_host(uint8_t * broadcast, uint8_t * max_host){
	int8_t carry = -1;
	for(int i = NUM_OCTETS - 1; i>=0;i--){
		if(carry==-1){
			carry = get_max_host(broadcast[i], max_host[i]);
		} else {
			max_host[i] = broadcast[i];
		}
	}
} 

string compile_output(uint8_t * ip_container,uint8_t * subnet_container,
		uint8_t * network_container,uint8_t * broadcast_container, 
		uint8_t *  min_host_container,uint8_t * max_host_container){
	string final_string = "";
	string ip = "IP Address: ";
	string subnet = "Subnet: ";
	string network = "Network: ";
	string broadcast = "Broadcast: ";
	string hostmin = "HostMin: ";
	string hostmax = "HostMax: ";

	int i;
	for(i =0;i<NUM_OCTETS-1;i++){
		ip+= to_string(ip_container[i]) + ".";
		subnet+= to_string(subnet_container[i]) + ".";
		network+= to_string(network_container[i]) + ".";
		broadcast+= to_string(broadcast_container[i]) + ".";
		hostmin+= to_string(min_host_container[i]) + ".";
		hostmax+= to_string(max_host_container[i]) + ".";
	}
	ip+= to_string(ip_container[i]) + "\n";
	subnet+= to_string(subnet_container[i]) + "\n";
	network+= to_string(network_container[i]) + "\n";
	broadcast+= to_string(broadcast_container[i]) + "\n";
	hostmin+= to_string(min_host_container[i]) + "\n";
	hostmax+= to_string(max_host_container[i]) + "\n";

	final_string+= ip;
	final_string+= subnet;
	final_string+= network;
	final_string+= broadcast;
	final_string+= hostmin;
	final_string+= hostmax;

	int total_zeros = 0;
	for(int i = 0;i<NUM_OCTETS;i++){
		total_zeros += count_zero_bits(subnet_container[i]);	
	}

	int num_hosts = calculate_number_of_hosts(total_zeros); 

	string hosts = "# Host: " + to_string(num_hosts);

	final_string+=hosts;

	return final_string;
} 

string start_process(string input_string){
	/*
	 * This function takes a raw string and calculate 
	 * @param
	 * 	string input_string: ex: 192.168.1.1 255.255.255.0   
	 * */
	string output_string = ""; 
	
	string ip_string = "";
	string subnet_string = "";

	split(input_string, ip_string, subnet_string);

	uint8_t ip_container[NUM_OCTETS];
	uint8_t subnet_container[NUM_OCTETS];
	uint8_t network_container[NUM_OCTETS];
	uint8_t broadcast_container[NUM_OCTETS];
	uint8_t min_host_container[NUM_OCTETS];
	uint8_t max_host_container[NUM_OCTETS];
	
	convert_to_int_array(ip_string, ip_container);
	convert_to_int_array(subnet_string, subnet_container);
	
	calcualte_network_thread(ip_container, subnet_container, network_container);
	calcualte_broadcast_thread(network_container, subnet_container, broadcast_container);
	calculate_min_host(network_container, min_host_container);
	calculate_max_host(broadcast_container, max_host_container);

	string final_string = compile_output(ip_container, subnet_container, 
			network_container, broadcast_container, 
			min_host_container, max_host_container);
	return final_string;
}

struct shmarg{
	uint8_t address[NUM_ADDRESS][NUM_OCTET]; 
};


int main(){

	const int COUNT = 5;

	string line1 = "192.168.1.1 255.255.255.0";
	string line2 = "10.0.127.24 255.255.255.240";
	string line3 = "172.17.0.1 255.240.0.0";
	string line4 = "127.24.35.254 255.192.0.0";
	string line5 = "192.168.243.24 255.255.248.0";
	
	string lines[COUNT] = {line1, line2, line3, line4, line5};
	string lines_result[COUNT];

	int fd[COUNT][2];
	int pids[COUNT];

	key_t key = ftok( "shared_mem", share_key);

	bool initialized_shared_mem = false;

	for(int i = 0;i<COUNT;i++){
		pipe(fd[i]);	
		pids[i] = fork();
		if(pids[i] > 0){

			//shmseg* data = new shmseg(COUNT);
			if(initialized_shared_mem==false){
				initialized_shared_mem = true;

				cout<<"Size "<<sizeof(char[COUNT][MAX_STRING_LENGTH])<<endl;
				int shmid = shmget(share_key, sizeof(char[COUNT][MAX_STRING_LENGTH]), 0666|IPC_CREAT);

				char ** head; 

				cout<<"SHMID "<< shmid <<endl;
				head  = (char**)shmat(shmid, NULL, 0);

				string something = "Hello World LULU";
				for(int i = 0;i<COUNT;i++){
					//head[i] = (char*)something.c_str();
					head[i] = (char*)something.c_str();
				}  

				shmdt(head);
			}

			close(fd[i][READ_FD]);

			int n = write(fd[i][WRITE_FD], &i, sizeof(int));	
			if(n == 0){
				cout<<"There's an error occured in pipe"<<endl;
				return -1;	
			}
			close(fd[i][WRITE_FD]);
			wait(NULL);
		} else if(pids[i]== 0){

			close(fd[i][WRITE_FD]);
			string read_str;	
			int data = 0;
			read(fd[i][READ_FD], &data, sizeof(int));

			int shmid = shmget(share_key, sizeof(char[COUNT][MAX_STRING_LENGTH]), 0666|IPC_CREAT);

			char ** head; 

			head  = (char**)shmat(shmid, NULL, 0);

			string final_string = start_process(lines[i]);

			cout<<"String length "<<final_string.length() << endl;

			string b = "Something + " + to_string(data);

			cout<<endl;

			//cout<<"Write "<<b<<" at data = "<<data<<endl;

			close(fd[i][READ_FD]); 
			shmdt(head);
			exit(1);
		}
	}

	//Wait for all child to coplete
	for(int i = 0;i< COUNT;i++){
		waitpid(pids[i], NULL, 0);
	}	

	int shmid = shmget(share_key, sizeof(char[COUNT][MAX_STRING_LENGTH]), 0666|IPC_CREAT);

	char ** head = (char **) shmat(shmid, NULL, 0);

	for(int i = 0;i < COUNT;i++){
		cout<<"Print "<<i<<endl;
		cout<< head[i] <<endl;
	}

	shmdt(head);

	shmctl(shmid, IPC_RMID, NULL);

	return 0;
}

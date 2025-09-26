    /*
        Original author of the starter code
        Tanzir Ahmed
        Department of Computer Science & Engineering
        Texas A&M University
        Date: 2/8/20
        
        Please include your Name, UIN, and the date below
        Name: Danny Lin
        UIN: 833007670
        Date: 9/26/2025
    */

    #include <iostream>
    #include <fstream>
    #include <string>
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/stat.h>
    #include <cstring>
    #include <iomanip>
    #include <vector>
    #include <chrono>
    #include "common.h"
    #include "FIFORequestChannel.h"

    using namespace std;

    int main (int argc, char *argv[]) {
        int opt;
        int patient_id = -1;
        double time_val = -1.0;
        int ecg_no = -1;
        string filename = "";
        int buffercapacity = MAX_MESSAGE;
        bool new_channel_flag = false;

        // P2, P3, P4: Collect command-line arguments
        // The `getopt` loop processes the arguments to set the appropriate variables.
        while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
            switch (opt) {
                case 'p':
                    // P2: `datamsg` arguments (-p)
                    patient_id = atoi(optarg);
                    break;
                case 't':
                    // P2: `datamsg` arguments (-t)
                    time_val = atof(optarg);
                    break;
                case 'e':
                    // P2: `datamsg` arguments (-e)
                    ecg_no = atoi(optarg);
                    break;
                case 'f':
                    // P3: File request argument (-f)
                    filename = optarg;
                    break;
                case 'm':
                    // P3, P4: Buffer capacity argument (-m)
                    buffercapacity = atoi(optarg);
                    break;
                case 'c':
                    // P4: New channel request flag (-c)
                    new_channel_flag = true;
                    break;
                case '?':
                    fprintf(stderr, "Invalid option: -%c\n", optopt);
                    return 1;
            }
        }

        // P1: Run the server as a child process
        // This `fork()` call creates a new process for the server.
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
        
        if (pid == 0) { // This is the child process (server)
            // P1: Use `execvp` to replace the child process with the server program.
            string mstr = to_string(buffercapacity);
            char *args[4];
            args[0] = (char*)"./server";
            args[1] = (char*)"-m";
            args[2] = (char*)mstr.c_str();
            args[3] = nullptr;
            execvp(args[0], args);
            perror("execvp failed");
            _exit(1);
        }

        // P1: The parent process (client) continues here
        // A small delay to give the server time to set up the named pipes.
        usleep(1000); 

        // P4: Create and possibly switch to a new channel
        // `main_channel` is a pointer to the channel object, allowing it to be changed later.
        FIFORequestChannel* control_channel = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
        FIFORequestChannel* data_channel = control_channel;

        if (new_channel_flag) {
            // P4: Send a NEWCHANNEL_MSG to the server.
            MESSAGE_TYPE new_chan_msg = NEWCHANNEL_MSG;
            control_channel->cwrite((char*)&new_chan_msg, sizeof(MESSAGE_TYPE));
            
            // P4: Read the new channel's name from the server.
            char new_chan_name[100];
            control_channel->cread(new_chan_name, 100);
            
            // P4: Create the new channel object using the name.
            data_channel = new FIFORequestChannel(new_chan_name, FIFORequestChannel::CLIENT_SIDE);
            
            cout << "Switched to new channel: " << new_chan_name << endl;
        }
        
        // P2, P3, P4: Run client logic on the appropriate channel
        // The code below now uses the `data_channel` pointer for all communication.

        // Q2: Requesting Data Points
        if (patient_id != -1 && time_val != -1.0 && ecg_no != -1) {
            // P2: Request one data point from the server.
            datamsg dmsg(patient_id, time_val, ecg_no);
            
            // P2: Serialize the message into a buffer using `memcpy`.
            char request_buf[sizeof(datamsg)]; 
            memcpy(request_buf, &dmsg, sizeof(datamsg)); // takes in bytes
            data_channel->cwrite(request_buf, sizeof(datamsg));

            double reply;
            data_channel->cread(&reply, sizeof(double));
            cout << "For person " << patient_id << ", at time " << time_val 
                << ", the value of ecg " << ecg_no << " is " << reply << endl;
        }
        else if (patient_id != -1) {
            // P2: Request 1000 data points and write to `x1.csv`.
            mkdir("received", 0777); 
            ofstream outfile("received/x1.csv"); // where im writing to
            if (!outfile.is_open()) {
                cerr << "Error: Could not open received/x1.csv for writing." << endl;
                return 1;
            }

            for (int i = 0; i < 1000; ++i) {
                double current_time = i * 0.004;

                datamsg dmsg1(patient_id, current_time, 1);
                char request_buf1[sizeof(datamsg)];
                memcpy(request_buf1, &dmsg1, sizeof(datamsg));
                data_channel->cwrite(request_buf1, sizeof(datamsg));
                
                double ecg1_val;
                data_channel->cread(&ecg1_val, sizeof(double));

                datamsg dmsg2(patient_id, current_time, 2);
                char request_buf2[sizeof(datamsg)];
                memcpy(request_buf2, &dmsg2, sizeof(datamsg));
                data_channel->cwrite(request_buf2, sizeof(datamsg));

                double ecg2_val;
                data_channel->cread(&ecg2_val, sizeof(double));
                
                outfile << std::defaultfloat << setprecision(10) << current_time << "," 
                        << ecg1_val << "," << ecg2_val << endl;
            }
            outfile.close();
            cout << "Successfully wrote 1000 data points to x1.csv" << endl;
        }
        // Q3: Requesting Files
        else if (!filename.empty()) {
            mkdir("received", 0777);
            auto start = chrono::high_resolution_clock::now(); // to track the time

            // P3: Request the file size by sending a filemsg with offset=0 and length=0.
            filemsg fsize_msg(0, 0);
            int req_len = sizeof(filemsg) + filename.size() + 1;
            
            // P3: Use `memcpy` to build the request buffer for the file size.
            vector<char> request_buf(req_len);
            memcpy(request_buf.data(), &fsize_msg, sizeof(filemsg));
            memcpy(request_buf.data() + sizeof(filemsg), filename.c_str(), filename.size() + 1);

            data_channel->cwrite(request_buf.data(), req_len);
            
            __int64_t file_size;
            data_channel->cread((char*)&file_size, sizeof(__int64_t));
            cout << "File size is: " << file_size << " bytes" << endl;
            
            // P3: Open the output file in binary mode (`"wb"`).
            string output_filepath = "received/" + filename;
            FILE* outfile = fopen(output_filepath.c_str(), "wb");
            if (!outfile) {
                cerr << "Error: Could not open output file " << output_filepath << endl;
                return 1;
            }

            __int64_t remaining_bytes = file_size;
            __int64_t current_offset = 0;
            
            // P3: Loop to request the file in chunks based on buffer capacity.
            while (remaining_bytes > 0) {
                int length = min((__int64_t)buffercapacity, remaining_bytes);
                
                // P3: Create a `filemsg` for the current chunk.
                filemsg f_msg(current_offset, length);
                int req_len_data = sizeof(filemsg) + filename.size() + 1;
                vector<char> request_buf_data(req_len_data);
                
                // P3: Build the request buffer for the chunk transfer.
                memcpy(request_buf_data.data(), &f_msg, sizeof(filemsg));
                memcpy(request_buf_data.data() + sizeof(filemsg), filename.c_str(), filename.size() + 1);
                
                data_channel->cwrite(request_buf_data.data(), req_len_data);
                
                // P3: Read the data chunk from the server.
                vector<char> response_buf(buffercapacity);
                data_channel->cread(response_buf.data(), length);
                
                // P3: Write the received data to the file.
                fwrite(response_buf.data(), 1, length, outfile);

                current_offset += length;
                remaining_bytes -= length;
            }

            fclose(outfile);
            cout << "Successfully transferred file: " << filename << endl;

            auto end = chrono::high_resolution_clock::now();
            chrono::duration<double> duration = end - start;

            cout << "Time taken to transfer " << file_size << " bytes: " << fixed 
            << setprecision(6) << duration.count() << " seconds" << endl;
        }
        
        // P5: Close channels and wait for the server
        // Send QUIT_MSG to the data channel, which may be the new channel.
        MESSAGE_TYPE m = QUIT_MSG;
        data_channel->cwrite((char*)&m, sizeof(MESSAGE_TYPE));
        
        // P5: Clean up memory for both channels.
        if (new_channel_flag) {
            delete data_channel;
            control_channel->cwrite((char*)&m, sizeof(MESSAGE_TYPE));
            delete control_channel;
        } else {
            delete control_channel;
        }
        
        // P5: Wait for the child server process to terminate.
        wait(NULL);

        cout << "Client-side is done and exited." << endl;
        return 0;
    }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.c"
#include "helper.h"
#include <sys/wait.h>
#include <limits.h>

int find_num_records(int size, int position, int processes)
{
	int total = size / sizeof(struct rec);
	int num = (total + processes - 1) / processes;
	for (int i = 0; i < position; i++)
	{
		if ((total) % (processes - i) == 0)
		{
			num = (total) / (processes - i);
		}
		else
		{
			num = (total + processes - i - 1) / (processes - i);
		}
		total = total - num;
	}

	return num;
}

struct rec *sorted_struct_sublist(int size_of_file, int num_of_prcs, int num_records, int position, char *filename)
{
	// Find the number of records to skip in the file to start reading
	int num_records_to_skip = 0;
	for (int i = 1; i < position; i++)
	{
		int prev_num = find_num_records(size_of_file, i, num_of_prcs);
		num_records_to_skip = num_records_to_skip + prev_num;
	}

	// Read and store the records in the array
	struct rec *records = malloc(sizeof(struct rec) * num_records);
	if (!records)
	{
		perror("malloc");
		exit(1);
	}
	FILE *fp = fopen(filename, "r");
	fseek(fp, (sizeof(struct rec) * num_records_to_skip), SEEK_SET);
	for (int j = 0; j < num_records; j++)
	{
		if (fread(&records[j], sizeof(struct rec), 1, fp) != 1)
		{
			perror("Failed to read from the child\n");
			exit(1);
		}
	}

	fclose(fp);
	qsort(records, num_records, sizeof(struct rec), compare_freq); // Sorting the array

	return records;
}

int get_opt_helper(int num_arg, char *arg_arr[], char *in_file, char *out_file, int *num)
{
	int opt;
	int flags = 0;
	while ((opt = getopt(num_arg, arg_arr, "n:f:o:")) != -1)
	{
		switch (opt)
		{
		case 'n':
			if (flags == 0)
			{
				*num = atoi(optarg);
				flags = 1;
				break;
			}
			fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
			exit(1);

		case 'f':
			if (flags == 1)
			{
				strcpy(in_file, optarg);
				flags = 2;
				break;
			}
			fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
			exit(1);

		case 'o':
			if (flags == 2)
			{
				strcpy(out_file, optarg);
				break;
			}
			fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
			exit(1);

		default:
			fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
			exit(1);
		}
	}

	return 0;
}

int merge(char *out_file, int pipe_arr[][2], int file_size, int num_of_prcs)
{
	// Open the output file
	FILE *out_fp = fopen(out_file, "wb");

	// Read from all the pipes at the same time and choose the smallest one first
	struct rec *temp_array = malloc(sizeof(struct rec) * (num_of_prcs));
	if (!temp_array)
	{
		perror("malloc");
		exit(1);
	}
	int num_records = (int)file_size / sizeof(struct rec);

	// Populate the temp_array first
	int arr[2][num_of_prcs];
	for (int i = 0; i < num_of_prcs; i++)
	{
		if (read(pipe_arr[i][0], &temp_array[i], sizeof(struct rec)) == -1)
		{
			perror("Reading from pipe in parent");
			exit(1);
		}
		arr[0][i] = find_num_records(file_size, i + 1, num_of_prcs);
		arr[1][i] = 0;
	}

	// Repopulate the array once an element was written to the file
	int k = 0;
	int smallest = INT_MAX;
	int index_smallest = INT_MAX;
	while (k < num_records)
	{
		for (int j = 0; j < num_of_prcs; j++)
		{
			if (temp_array[j].freq < smallest && arr[1][j] != arr[0][j])
			{
				smallest = temp_array[j].freq;
				index_smallest = j;
			}
		}
		if (fwrite(&temp_array[index_smallest], sizeof(struct rec), 1, out_fp) != 1)
		{
			perror("Failed to write to the pipe\n");
			exit(1);
		}
		if (read(pipe_arr[index_smallest][0], &temp_array[index_smallest], sizeof(struct rec)) == -1)
		{
			perror("Reading from pipe in parent");
			exit(1);
		}
		smallest = INT_MAX;
		arr[1][index_smallest]++;
		k++;
	}
	fclose(out_fp);
	free(temp_array);
	return 0;
}

int main(int argc, char *argv[])
{
	// Declaring variables to perform getopt operation
	char *input_filename = malloc(sizeof(char) * 100);
	if (!input_filename)
	{
		perror("malloc");
		exit(1);
	}
	char *output_filename = malloc(sizeof(char) * 100);
	if (!output_filename)
	{
		perror("malloc");
		exit(1);
	}
	int *num_of_prcs = malloc(sizeof(int));
	if (!num_of_prcs)
	{
		perror("malloc");
		exit(1);
	}
	get_opt_helper(argc, argv, input_filename, output_filename, num_of_prcs);

	// This part of the code is to create pipes(number of pipes = number of processes)
	// and fork n children
	// Declaring all the needed variables to fork and create a pipe for each child
	int pipe_fd[*num_of_prcs][2];
	int file_size = get_file_size(input_filename);

	// Check if number of processes to avoid crashes
	if (*num_of_prcs == 0 || file_size == 0) // Case when number of processes is 0 or the size of the file is 0
	{
		printf("The number of processes is 0 or the input file is 0. Hence exiting with 0\n");
		exit(0);
	}
	else if (*num_of_prcs > file_size / sizeof(struct rec)) // Case when the number of processes is bigger than the num of records
	{
		*num_of_prcs = file_size / sizeof(struct rec);
	}
	else if (*num_of_prcs < 0) // Case when the number of processes is smaller than 0
	{
		*num_of_prcs = 1;
	}

	for (int i = 1; i < *num_of_prcs + 1; i++)
	{
		// Call pipe before we fork
		if ((pipe(pipe_fd[i - 1])) == -1)
		{
			perror("pipe");
			exit(1);
		}
		// Call fork
		int result = fork();
		if (result < 0)
		{ // Case: a system call error -> handle the error
			perror("fork");
			exit(1);
		}
		else if (result == 0)
		{ // Case: a child process
			// Every child does its work here
			// Child only writes to the pipe so close the read end
			if (close(pipe_fd[i - 1][0]) == -1)
			{
				perror("Close the read end of the pipe for the child");
				exit(1);
			}

			// Close the read end for previously forked children
			for (int j = 0; j < i - 1; j++)
			{
				if (close(pipe_fd[j][0]) == -1)
				{
					perror("Close the read end of the pipe for the child");
					exit(1);
				}
			}

			// THE CHILD DOES ITS WORK HERE
			// Create a list of structs so that we can sort it and write to the pipe
			int num_of_records = find_num_records(file_size, i, *num_of_prcs);

			struct rec *struct_sublist;
			struct_sublist = sorted_struct_sublist(file_size, *num_of_prcs, num_of_records, i, input_filename); // Calling a helper function to create a sorted sublist

			// Wrtite the sorted sublist to the pipe
			for (int j = 0; j < num_of_records; j++)
			{
				if (write(pipe_fd[i - 1][1], &struct_sublist[j], sizeof(struct rec)) != sizeof(struct rec))
				{
					perror("Child writting to the pipe");
					exit(1);
				}
			}

			// Close the pipe after the work is done
			if (close(pipe_fd[i - 1][1]) == -1)
			{
				perror("Close child pipe after writing");
				exit(1);
			}

			free(struct_sublist); // Freeing the space that we malloced in our helper function
			exit(0);
		}
	}

	// THE PARENT DOES ALL ITS WORK HERE
	// Close the write end of the pipe for parent
	for (int i = 1; i < *num_of_prcs + 1; i++)
	{
		if (close(pipe_fd[i - 1][1]) == -1)
		{
			perror("Close write end for parent");
			exit(1);
		}
	}
	// Call the merge function to merge everyhting from the children
	merge(output_filename, pipe_fd, file_size, *num_of_prcs);

	// close the read end in the parent for every child
	for (int i = 1; i < *num_of_prcs + 1; i++)
	{
		if (close(pipe_fd[i - 1][0]) == -1)
		{
			perror("Close the read end for parent");
			exit(1);
		}
	}

	// Freeing all the memory that was allocated
	free(input_filename);
	free(output_filename);

	// Check if children have terminated properly
	int status;
	for (int i = 1; i < *num_of_prcs + 1; i++)
	{
		if (wait(&status) == -1)
		{
			perror("wait");
			exit(1);
		}
		if (!WIFEXITED(status))
		{
			fprintf(stderr, "Child terminated abnormally\n");
		}
	}
	free(num_of_prcs);
	return 0;
}